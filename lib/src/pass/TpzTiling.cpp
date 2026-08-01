/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <algorithm>
#include <climits>
#include <functional>
#include <numeric>
#include <set>
#include <vector>

#include <isl/aff.h>
#include <isl/ast_build.h>
#include <isl/flow.h>
#include <isl/maybe_pw_aff.h>
#include <isl/multi.h>
#include <isl/options.h>
#include <isl/printer.h>
#include <isl/schedule.h>
#include <isl/schedule_node.h>
#include <isl/set.h>
#include <isl/union_map.h>
#include <isl/union_set.h>
#include <isl/val.h>

#include <cloog/isl/cloog.h>

#include <trahrhe/trahrhe.h>

#include <pesto/Pluto.hpp>
#include <pesto/clast_support.h>
#include <pesto/codegen/ParamTiling.hpp>
#include <pesto/ir/Isl2Scop.hpp>
#include <pesto/ir/Scop2Isl.hpp>
#include <pesto/maths/Maths.hpp>
#include <pesto/metadata/FileInfo.hpp>
#include <pesto/metadata/Macro.hpp>
#include <pesto/metadata/VarDecl.hpp>
#include <pesto/pass/CloogCodegen.hpp>
#include <pesto/pass/Pluto.hpp>
#include <pesto/pass/TpzTiling.hpp>

using pesto::Bundle;
using pesto::pass::TpzTilingPass;
using pesto::pass::ptile::ClastTilingPair;

TpzTilingPass::TpzTilingPass() : IPass("tpz-tiling") {
	this->ctx = isl_ctx_alloc();
	isl_options_set_on_error(this->ctx, ISL_ON_ERROR_ABORT);
	this->printer = isl_printer_to_str(this->ctx);
}

TpzTilingPass::~TpzTilingPass() {
	isl_printer_free(this->printer);
	isl_ctx_free(this->ctx);
}

namespace pesto::pass {} // namespace pesto::pass

static bool hasScalarWrite(osl_statement_p stmt);
static void collectScalarWrites(osl_scop_p scop, std::vector<std::string> &names);

Bundle TpzTilingPass::apply(Bundle &&bundle) {
	auto *scopIR = dynamic_cast<pesto::ir::OpenScopIR *>(bundle.get_ir());
	osl_scop_p scop = scopIR->c_scop();

	this->scalarWrites_.clear();
	collectScalarWrites(scop, this->scalarWrites_);

	logger->debug("Starting trapezoidal tiling pass on scop");

	logger->debug("Computing dependency distance vectors");
	std::optional<pesto::maths::Vec2i> slope = this->computeDepDistanceVector(scop);

	if (!slope.has_value()) {
		this->logger->error("Trapezoidal tiling cannot be performed on this scop.");
		throw std::runtime_error("Trapezoidal tiling cannot be performed on this scop");
	}

	this->zTileable_ = this->isReductionTileable(scop);

	this->hoistingLegal_ = this->isHoistingLegal(scop);

	this->logger->debug("Steepest dependency vector: ({}, {})", slope->x, slope->y);
	this->logger->debug("Trapezoidal tiling slope: {}", slope->y / slope->x);

	// Generate a Cloog ast from the scop
	GenClastPass genClastPass;
	genClastPass.cloogF = -1;
	genClastPass.cloogL = 1;
	// TODO: should not be hardcoded
	genClastPass.codegenContext = 10; // simplify ast

	bundle = genClastPass.apply(std::move(bundle));
	if (bundle.empty()) {
		logger->error("Clast generation failed");
		throw std::runtime_error("Clast generation failed");
	}
	if (bundle.get_ir()->id() != pesto::ir::ClastIR::getID()) {
		logger->error("Expected ClastIR after GenClastPass, got {}", bundle.get_ir()->id().c_str());
		throw std::runtime_error("Unexpected IR after GenClastPass");
	}
	auto *clastIR = dynamic_cast<pesto::ir::ClastIR *>(bundle.get_ir());
	struct clast_root *root = (struct clast_root *)clastIR->get();

	auto stmts = clast_get_user_stmt_list(&(root->stmt), &this->insertedStmts);
	free(stmts);

	std::function<struct clast_for *(struct clast_stmt *)> findFirstFor = [&](struct clast_stmt *
																			  s) -> struct clast_for * {
		for (; s != NULL; s = s->next) {
			if (CLAST_STMT_IS_A(s, stmt_for))
				return (struct clast_for *)s;
			if (CLAST_STMT_IS_A(s, stmt_guard))
				if (auto *f = findFirstFor(((struct clast_guard *)s)->then))
					return f;
			if (CLAST_STMT_IS_A(s, stmt_block))
				if (auto *f = findFirstFor(((struct clast_block *)s)->body))
					return f;
		}
		return nullptr;
	};
	if (struct clast_for *clastFor = findFirstFor(&(root->stmt)))
		tileClastFor(bundle, clastFor, slope.value());
	else
		logger->error("No for loop found to tile");

	return std::move(bundle);
}

bool TpzTilingPass::isHoistingLegal(osl_scop_p scop) {
	using pesto::ir::converter::Scop2Isl;
	Scop2Isl irHelper(this->ctx);
	irHelper.convert(scop);

	isl_union_map *schedules = irHelper.getSchedules();
	isl_union_map *writes = irHelper.getWrites();
	isl_union_map *reads = irHelper.getReads();
	isl_union_map *mayWrites = irHelper.getMayWrites();
	isl_union_set *domains = irHelper.getDomains();

	// Une instruction qui ne s'étend pas sur la dimension de réduction a moins de
	// dimensions de boucle que les plus profondes : ce sont celles que la scission sort.
	struct Collect {
		unsigned maxDim;
		std::vector<std::pair<std::string, unsigned>> stmts;
	};
	Collect col{0, {}};
	isl_union_set_foreach_set(
		domains,
		+[](__isl_take isl_set *s, void *user) -> isl_stat {
			auto *c = static_cast<Collect *>(user);
			unsigned d = isl_set_dim(s, isl_dim_set);
			if (const char *n = isl_set_get_tuple_name(s))
				c->stmts.push_back({n, d});
			if (d > c->maxDim)
				c->maxDim = d;
			isl_set_free(s);
			return isl_stat_ok;
		},
		&col);

	std::set<std::string> hoisted;
	for (auto &p : col.stmts)
		if (p.second < col.maxDim)
			hoisted.insert(p.first);

	bool legal = true;
	if (!hoisted.empty()) {
		isl_map_list *sl = isl_union_map_get_map_list(schedules);
		isl_union_map *named = isl_union_map_empty_ctx(this->ctx);
		for (isl_size i = 0; i < isl_map_list_n_map(sl); ++i) {
			isl_map *m = isl_map_list_get_at(sl, i);
			m = isl_map_set_tuple_name(m, isl_dim_out, isl_map_get_tuple_name(m, isl_dim_in));
			named = isl_union_map_add_map(named, m);
		}
		isl_map_list_free(sl);

		isl_union_map *allWrites = isl_union_map_union(isl_union_map_copy(writes), isl_union_map_copy(mayWrites));
		auto deps = [&](isl_union_map *sink, isl_union_map *src) {
			isl_union_access_info *ai = isl_union_access_info_from_sink(isl_union_map_copy(sink));
			ai = isl_union_access_info_set_may_source(ai, isl_union_map_copy(src));
			ai = isl_union_access_info_set_schedule_map(ai, isl_union_map_copy(schedules));
			isl_union_flow *f = isl_union_access_info_compute_flow(ai);
			isl_union_map *d = isl_union_flow_get_may_dependence(f);
			isl_union_flow_free(f);
			return d;
		};
		isl_union_map *all = deps(reads, allWrites);
		all = isl_union_map_union(all, deps(allWrites, reads));
		all = isl_union_map_union(all, deps(allWrites, allWrites));
		isl_union_map_free(allWrites);
		all = isl_union_map_apply_range(all, isl_union_map_copy(named));
		all = isl_union_map_apply_domain(all, isl_union_map_copy(named));

		isl_map_list *dl = isl_union_map_get_map_list(all);
		for (isl_size i = 0; i < isl_map_list_n_map(dl); ++i) {
			isl_map *m = isl_map_list_get_at(dl, i);
			const char *src = isl_map_get_tuple_name(m, isl_dim_in);
			const char *snk = isl_map_get_tuple_name(m, isl_dim_out);
			if (src && snk && hoisted.count(snk) && !hoisted.count(src) && isl_map_dim(m, isl_dim_in) > 3 &&
				isl_map_dim(m, isl_dim_out) > 3) {
				// La scission n'inverse que les paires à même gap (dim 1) dont le
				// puits est à un t4 (dim 3) strictement plus grand que la source.
				isl_local_space *ls = isl_local_space_from_space(isl_map_get_space(m));
				isl_constraint *eq = isl_equality_alloc(isl_local_space_copy(ls));
				eq = isl_constraint_set_coefficient_si(eq, isl_dim_out, 1, 1);
				eq = isl_constraint_set_coefficient_si(eq, isl_dim_in, 1, -1);
				isl_constraint *gt = isl_inequality_alloc(ls);
				gt = isl_constraint_set_coefficient_si(gt, isl_dim_out, 3, 1);
				gt = isl_constraint_set_coefficient_si(gt, isl_dim_in, 3, -1);
				gt = isl_constraint_set_constant_si(gt, -1);
				isl_map *bad = isl_map_add_constraint(isl_map_copy(m), eq);
				bad = isl_map_add_constraint(bad, gt);
				if (isl_map_is_empty(bad) == isl_bool_false) {
					logger->info("Hoisting {} is illegal: it depends on {} at a smaller t4 "
								 "within the same gap",
								 snk, src);
					legal = false;
				}
				isl_map_free(bad);
			}
			isl_map_free(m);
		}
		isl_map_list_free(dl);
		isl_union_map_free(all);
		isl_union_map_free(named);
	}

	isl_union_map_free(schedules);
	isl_union_map_free(writes);
	isl_union_map_free(reads);
	isl_union_map_free(mayWrites);
	isl_union_set_free(domains);
	logger->debug("Hoisting legal: {}", legal);
	return legal;
}

void TpzTilingPass::tileClastFor(Bundle &bundle, struct clast_for *clastFor, pesto::maths::Vec2i slope) {
	using pesto::pass::ptile::ClastTilingPair;
	using namespace pesto::bundle;

	assert(bundle.get_ir()->id() == pesto::ir::ClastIR::getID());
	auto *clastIR = dynamic_cast<pesto::ir::ClastIR *>(bundle.get_ir());

	struct clast_root *root = (struct clast_root *)clastIR->get();

	auto &macroData = bundle.metadata.get<metadata::Macro>();

	this->logger->debug("tiling clastFor: {}", clastFor->iterator);
	std::vector<ClastTilingPair> clastTilingPairs = this->computeTilingPairs(*clastIR, clastFor);

	isl_set *innerDomain = this->computeInnerDomain(*clastIR, clastTilingPairs);

	// PHIL: commented by me
	// Rename scattering dimensions to t2/t4/t6 once, before both codegen (which
	// emits the gen_maxcut comment) and Trahrhe header generation consume the
	// domain. The multi-statement domain reconstruction (PickMax + convex_hull)
	// can leave the innermost dimension anonymous (printed as i2); Trahrhe writes
	// these names with fputs and segfaults on a null one.
	/*{
		int nd = isl_set_dim(innerDomain, isl_dim_set);
		for (int i = 0; i < nd; i++) {
			std::string nm = "t" + std::to_string(2 * (i + 1)); // t2, t4, t6
			innerDomain = isl_set_set_dim_name(innerDomain, isl_dim_set, i, nm.c_str());
		}
	} */

	// isl_printer_print_set asserts on a null tuple name; the PickMax
	// reconstruction cleared it. Give the domain a tuple name.
	if (!isl_set_get_tuple_name(innerDomain))
		innerDomain = isl_set_set_tuple_name(innerDomain, "T");

	if (logger->level() <= spdlog::level::debug) {
		this->printer = isl_printer_print_set(this->printer, innerDomain);
		char *set_str = isl_printer_get_str(printer);
		logger->debug("Computed inner domain: {}", set_str);
		free(set_str);
		this->printer = isl_printer_flush(this->printer);
	}

	// La boucle de tuile de z enveloppe la boucle point : tout ce que contient son
	// corps serait rejoué à chaque tuile. Les instructions qui ne portent pas la
	// dimension de réduction sortent donc dans une copie de la boucle point, placée
	// avant elle. L'original garde la réduction, de sorte que
	// clastTilingPairs[1].innerFor reste la boucle à envelopper.
	if (clastTilingPairs.size() == 3 && clastTilingPairs[2].underGap) {
		struct clast_for *point = clastTilingPairs[1].innerFor;
		struct clast_for *red = clastTilingPairs[2].innerFor;

		// Position de la boucle de réduction dans le corps, et taille du corps.
		int redIdx = -1, nbStmts = 0;
		for (struct clast_stmt *s = point->body; s != NULL; s = s->next, ++nbStmts)
			if (CLAST_STMT_IS_A(s, stmt_for) && (struct clast_for *)s == red)
				redIdx = nbStmts;
		bool hasPre = redIdx > 0;
		bool hasPost = redIdx >= 0 && redIdx < nbStmts - 1;
		bool needsSplit = hasPre || hasPost;

		// Each side has its own condition: hoisting above the tiles is what
		// hoistingLegal_ tests; moving finalisations below them requires instead
		// that no same-gap dependence be carried by z, which is underGapLegal_.
		if ((hasPre && !this->hoistingLegal_) || (hasPost && !this->underGapLegal_)) {
			logger->info("Reduction not tiled: the point loop cannot be split");
			clastTilingPairs.resize(2);
		} else if (needsSplit) {
			// garde les éléments d'indice dans [lo, hi] ; les autres sont détachés
			// et laissés en place (pas d'API de libération de clast).
			auto keepRange = [](struct clast_stmt * head, int lo, int hi) -> struct clast_stmt * {
				struct clast_stmt *newHead = NULL, *tail = NULL, *s = head;
				int i = 0;
				while (s != NULL) {
					struct clast_stmt *next = s->next;
					if (i >= lo && i <= hi) {
						s->next = NULL;
						if (tail)
							tail->next = s;
						else
							newHead = s;
						tail = s;
					}
					s = next;
					++i;
				}
				return newHead;
			};
			logger->debug("Point loop split: {} stmt(s) before the reduction, {} after", hasPre ? redIdx : 0,
						  hasPost ? nbStmts - 1 - redIdx : 0);
			// Finalisations : elles ont besoin de la réduction terminée, donc elles
			// vont APRÈS la boucle de tuiles (cholesky : /= et sqrt).
			if (hasPost) {
				struct clast_stmt *post = clast_stmt_hard_copy(&(point->stmt));
				((struct clast_for *)post)->body = keepRange(((struct clast_for *)post)->body, redIdx + 1, nbStmts - 1);
				post->next = point->stmt.next;
				point->stmt.next = post;
			}
			// Initialisations : hissées AVANT la boucle de tuiles.
			if (hasPre) {
				struct clast_stmt *pre = clast_stmt_hard_copy(&(point->stmt));
				((struct clast_for *)pre)->body = keepRange(((struct clast_for *)pre)->body, 0, redIdx - 1);
				clast_insert_before(&(root->stmt), &(point->stmt), pre);
			}
			// L'original ne garde que la réduction.
			point->body = keepRange(point->body, redIdx, redIdx);
		}
	}

	this->insertOuterForLoop(bundle, clastTilingPairs);

	// duplicate ast at depth 1
	ClastTilingPair &trapezoidalPair = clastTilingPairs[1];

	this->logger->debug("Duplicating outer for loop {}", trapezoidalPair.outerFor->iterator);
	// TODO: we might have to find a way to group user statement by pair of original and duplicated
	struct clast_stmt *secondTileBand = clast_stmt_hard_copy(&(trapezoidalPair.outerFor->stmt));
	trapezoidalPair.outerFor->stmt.next = secondTileBand;

	if (this->diagTail_) {
		logger->debug("re-emitting diagonal tail after the second front");
		struct clast_stmt *diagLoop = clast_stmt_hard_copy(&(clastTilingPairs[0].innerFor->stmt));
		((struct clast_for *)diagLoop)->body = this->diagTail_;
		diagLoop->next = secondTileBand->next;
		secondTileBand->next = diagLoop;
		this->diagTail_ = nullptr;
	} else {
		logger->debug("no diagonal tail to re-emit");
	}
	this->logger->debug("Duplication done");

	// replace the duplicated inner for user statements

	int dupClastUserStmtListSize = 0;
	clast_user_stmt **dupUserStmtList = clast_get_user_stmt_list(secondTileBand, &dupClastUserStmtListSize);

	// The duplicated band holds clones of the reduction tile loops. Register them
	// so insertInnerLoopBounds recognises their point loops as sitting under a
	// tile and gives them _tpzlb2/_tpzub2 rather than the full trapezoidal range.
	for (auto &pair : clastTilingPairs) {
		if (!pair.underGap)
			continue;
		size_t before = pair.outerFors.size();
		for (auto *b : pair.outerBodyStmts) {
			int num = b->statement->number;
			for (int j = 0; j < dupClastUserStmtListSize; ++j)
				if (dupUserStmtList[j]->statement->number == num) {
					struct clast_for *dupFor = clast_get_parent_clast_for(secondTileBand, &(dupUserStmtList[j]->stmt));
					if (dupFor)
						pair.outerFors.push_back(dupFor);
					break;
				}
		}
		logger->debug("duplicated band: {} reduction tile loop(s) registered", pair.outerFors.size() - before);
	}

	for (size_t i = 0; i < clastTilingPairs.size(); ++i) {
		ClastTilingPair &pair = clastTilingPairs[i];
		pair.innerBodyStmts1.clear();
		for (auto *orig : pair.innerBodyStmts0) {
			int targetStmtNumber = orig->statement->number;
			clast_user_stmt *targetStmt = nullptr;
			for (int j = 0; j < dupClastUserStmtListSize; ++j) {
				if (dupUserStmtList[j]->statement->number == targetStmtNumber) {
					targetStmt = dupUserStmtList[j];
					break;
				}
			}
			if (targetStmt == nullptr) {
				this->logger->error("Error: could not find duplicated user statement for number {}", targetStmtNumber);
				continue;
			}
			this->insertedStmts += 1;
			targetStmt->statement->number = this->insertedStmts;
			pair.innerBodyStmts1.push_back(targetStmt);
		}
		pair.innerBodyStmt1 = pair.innerBodyStmts1.empty() ? nullptr : pair.innerBodyStmts1[0];
	}

	free(dupUserStmtList);

	this->insertInnerLoopBounds(*clastIR, clastTilingPairs);
	this->insertOmpPragmas(*clastIR, clastTilingPairs);
	clast_root_develop_user_stmt_substitutions(root);

	this->codegen(bundle, clastTilingPairs, innerDomain, slope);

	/* if (logger->level() <= spdlog::level::debug) {
		this->printer = isl_printer_print_set(this->printer, innerDomain);
		char *set_str = isl_printer_get_str(printer);
		logger->debug("Starting trahrhe header generation using domain: {}", set_str);
		free(set_str);
		this->printer = isl_printer_flush(this->printer);
	} */

	// Note: this domain counts raw index values of the gap dimension, not
	// effective iterations — the parity of a strided gap (t1 += 2 under diamond
	// tiling) is deliberately left out. Adding a congruence would force Trahrhe
	// to produce quasi-polynomials with periodic coefficients, heavier to
	// evaluate and more fragile, for no benefit: the overcount cancels in the
	// slicing, since TARGET_VOL_L1 and max_cut_volume scale with it and the
	// Trahrhe inverse works on the same polynomial. It would matter only if the
	// count were ever compared to a real measure of work.

	this->generateTrahrheHeaders(bundle, innerDomain);
	logger->debug("Trahrhe header generation done.");

	for (std::size_t i = 0; i < clastTilingPairs.size(); ++i) {
		macroData.nonProcessedList.push_back("#ifndef DIV" + std::to_string(i) +
											 "\n"
											 "#define DIV" +
											 std::to_string(i) +
											 " 8\n"
											 "#endif");
	}

	isl_set_free(innerDomain);
}

std::vector<pesto::pass::ptile::ClastTilingPair> TpzTilingPass::computeTilingPairs(pesto::ir::ClastIR &clastIR,
																				   struct clast_for *clastFor) {
	using pesto::pass::ptile::ClastTilingPair;

	struct clast_root *root = (struct clast_root *)clastIR.get();
	std::vector<ClastTilingPair> clastTilingPairs;

	// clast_get_innermost_for s'arrête au premier élément du corps : quand celui-ci
	// est une instruction qui ne porte pas la dimension interne (une initialisation),
	// les niveaux qui n'existent que sous elle ne sont jamais proposés au pavage.
	struct clast_for *innermost = clastFor;
	unsigned bestDepth = 1;
	std::function<void(struct clast_stmt *, unsigned)> findDeepest = [&](struct clast_stmt *s, unsigned d) {
		for (; s != NULL; s = s->next) {
			if (CLAST_STMT_IS_A(s, stmt_for)) {
				struct clast_for *f = (struct clast_for *)s;
				if (d + 1 > bestDepth) {
					bestDepth = d + 1;
					innermost = f;
				}
				findDeepest(f->body, d + 1);
			} else if (CLAST_STMT_IS_A(s, stmt_guard)) {
				findDeepest(((struct clast_guard *)s)->then, d);
			} else if (CLAST_STMT_IS_A(s, stmt_block)) {
				findDeepest(((struct clast_block *)s)->body, d);
			}
		}
	};
	findDeepest(clastFor->body, 1);

	// init tiling pairs with inner for loops
	for (struct clast_for *for_it = innermost; for_it != NULL;
		 for_it = clast_get_parent_clast_for(&(root->stmt), &(for_it->stmt))) {
		ClastTilingPair pair;
		pair.innerFor = for_it;
		logger->debug("Found inner for: {}", for_it->iterator);
		clastTilingPairs.push_back(pair);
	}

	unsigned depth = 0;
	for (auto it = clastTilingPairs.rbegin(); it != clastTilingPairs.rend(); ++it) {
		it->depth = depth++;
	}

	std::sort(clastTilingPairs.begin(), clastTilingPairs.end(),
			  [](const ClastTilingPair &a, const ClastTilingPair &b) { return a.depth < b.depth; });
	// The reduction handling applies only where there is a reduction: statements
	// of differing depth. A pure stencil has a single depth, and its innermost
	// level is a space dimension — life and seidel-2d have three levels, 3d7pt
	// four — so every pair stacks above the point loops.
	if (this->hasReduction_ && clastTilingPairs.size() == 3) {
		if (this->scalarInReduction_) {
			// Correctness, not a placement preference: no form of z tiling works.
			clastTilingPairs.resize(2);
		} else if (this->reductionAboveGap_.has_value()) {
			clastTilingPairs[2].underGap = !(*this->reductionAboveGap_);
			if (!*this->reductionAboveGap_ && !this->underGapLegal_)
				logger->warn("reductionTileAboveGap=false forces the reduction tile under "
							 "the gap although the legality test says otherwise");
		} else if (!this->zTileable_) {
			if (this->underGapLegal_)
				clastTilingPairs[2].underGap = true;
			else
				clastTilingPairs.resize(2);
		}
	}
	return clastTilingPairs;
}

// A sibling that holds a loop on the reduction iterator is another reduction
// nest (lu's second point loop) and must stay inside the tile loop; only what
// follows such nests is a finalisation.
static bool containsReductionLoop(struct clast_stmt *s, const char *redIt) {
	if (!s || !redIt)
		return false;
	if (CLAST_STMT_IS_A(s, stmt_for)) {
		struct clast_for *f = (struct clast_for *)s;
		if (f->iterator && strcmp(f->iterator, redIt) == 0)
			return true;
		for (struct clast_stmt *b = f->body; b; b = b->next)
			if (containsReductionLoop(b, redIt))
				return true;
	} else if (CLAST_STMT_IS_A(s, stmt_guard)) {
		struct clast_guard *g = (struct clast_guard *)s;
		for (struct clast_stmt *b = g->then; b; b = b->next)
			if (containsReductionLoop(b, redIt))
				return true;
	} else if (CLAST_STMT_IS_A(s, stmt_block)) {
		struct clast_block *bl = (struct clast_block *)s;
		for (struct clast_stmt *b = bl->body; b; b = b->next)
			if (containsReductionLoop(b, redIt))
				return true;
	}
	return false;
}

void TpzTilingPass::insertOuterForLoop(Bundle &bundle, std::vector<ptile::ClastTilingPair> &clastTilingPairs) {
	using pesto::ptile::NameHelper;
	assert(bundle.get_ir()->id() == pesto::ir::ClastIR::getID());
	auto *clastIR = dynamic_cast<pesto::ir::ClastIR *>(bundle.get_ir());

	struct clast_root *root = (struct clast_root *)clastIR->get();
	CloogOptions *cloogOptions = clastIR->getCloogOptions();
	auto &varDecl = bundle.metadata.get<pesto::bundle::metadata::VarDecl>();
	struct clast_for *outermostFor = clastTilingPairs[0].innerFor;

	CloogDomain *enclosingDomain =
		cloog_domain_universe(cloogOptions->state, cloog_domain_dimension(outermostFor->domain));
	CloogDomain *innerEnclosingDomain = nullptr;
	if (clastTilingPairs.size() == 3)
		innerEnclosingDomain =
			cloog_domain_universe(cloogOptions->state, cloog_domain_dimension(clastTilingPairs[1].innerFor->domain));

	// Statements in the gap body that carry no point-loop iterator (cholesky's
	// diagonal sqrt) belong to no zt2 slice: each slice would re-execute them.
	// Detach them here; they are re-emitted after both fronts.
	{
		struct clast_for *gap = clastTilingPairs[0].innerFor;
		struct clast_for *point = clastTilingPairs[1].innerFor;
		const char *pit = point->iterator;
		struct clast_stmt *prev = nullptr;
		for (struct clast_stmt *s = gap->body; s != NULL; prev = s, s = s->next) {
			bool isPointLoop = CLAST_STMT_IS_A(s, stmt_for) && ((struct clast_for *)s)->iterator && pit &&
							   strcmp(((struct clast_for *)s)->iterator, pit) == 0;
			if (!isPointLoop) {
				this->diagTail_ = s;
				if (prev)
					prev->next = nullptr;
				else
					gap->body = nullptr;
				break;
			}
		}
		logger->debug("diagonal tail: {}", this->diagTail_ ? "detached" : "none found");
	}

	// do tiling by inserting outer for loops
	for (auto it = clastTilingPairs.begin(); it != clastTilingPairs.end(); ++it) {
		// The reduction tile loop goes inside t2, as a parent of the t4 point
		// loop; the other levels stack above the outermost point loop.
		// Several reduction nests can be siblings under the gap (lu's two point
		// loops): each needs its own tile loop, so that its lbt3/ubt3 are
		// recomputed instead of inheriting the previous nest's last tile.
		std::vector<struct clast_for *> targets;
		if (it->underGap) {
			const char *redIt = it->innerFor->iterator;
			for (struct clast_stmt *s = &(clastTilingPairs[1].innerFor->stmt); s; s = s->next)
				if (CLAST_STMT_IS_A(s, stmt_for) && containsReductionLoop(s, redIt))
					targets.push_back((struct clast_for *)s);
		} else {
			targets.push_back(outermostFor);
		}

		// The nests are sequential siblings, so their tile loops never coexist:
		// they share the same iterator and bound names, each recomputing them.
		const char *innerItName = it->innerFor->iterator;
		std::string innerItSafe = innerItName ? std::string(innerItName) : ("d" + std::to_string(it->depth));
		std::unique_ptr<std::string> it_name = std::make_unique<std::string>("z" + innerItSafe);
		NameHelper nameHelper = NameHelper(it->depth, *it_name, innerItSafe.c_str());
		std::unique_ptr<std::string> lbName = std::make_unique<std::string>(nameHelper.outerForLBName());
		std::unique_ptr<std::string> ubName = std::make_unique<std::string>(nameHelper.outerForUBName());
		logger->debug("Creating outer for loop with iterator: {} ({} nest(s))", it_name->c_str(), targets.size());
		varDecl.addVar(*it_name);
		varDecl.addVar(nameHelper.outerForLBName());
		varDecl.addVar(nameHelper.outerForUBName());

		for (size_t ti = 0; ti < targets.size(); ++ti) {
			struct clast_for *target = targets[ti];

			// clast_insert_enclosing_for wraps the target AND all its following
			// siblings; detach them so the wrap holds this nest only.
			struct clast_stmt *trailing = target->stmt.next;
			target->stmt.next = nullptr;
			struct clast_for *enclosing_for = clast_insert_enclosing_for(
				&(root->stmt), &(target->stmt), it->underGap ? innerEnclosingDomain : enclosingDomain);
			if (trailing)
				enclosing_for->stmt.next = trailing;

			enclosing_for->iterator = it_name->c_str();
			enclosing_for->LB = &(new_clast_name(lbName->c_str())->expr);
			enclosing_for->UB = &(new_clast_name(ubName->c_str())->expr);

			it->outerFors.push_back(enclosing_for);
			if (ti == 0)
				it->outerFor = enclosing_for;

			this->insertedStmts += 1;
			auto *hdr = pesto_clast_create_stmt(cloogOptions, nullptr, this->insertedStmts);
			clast_insert_before(&(root->stmt), &(enclosing_for->stmt), &(hdr->stmt));
			it->outerHeaderStmts.push_back(hdr);
			if (ti == 0)
				it->outerHeaderStmt = hdr;

			this->insertedStmts += 1;
			auto *bdy = pesto_clast_create_stmt(cloogOptions, nullptr, this->insertedStmts);
			clast_insert_indepth(&(enclosing_for->stmt), &(bdy->stmt));
			it->outerBodyStmts.push_back(bdy);
			if (ti == 0)
				it->outerBodyStmt = bdy;
		}

		this->stringKeepAlive.push_back(std::move(it_name));
		this->stringKeepAlive.push_back(std::move(lbName));
		this->stringKeepAlive.push_back(std::move(ubName));
		it->innerFor->parallel = CLAST_PARALLEL_NOT;

		{
			struct clast_for **loops = NULL;
			int nloops = 0;
			int *stmts = NULL;
			int nstmts = 0;
			ClastFilter filter = {it->innerFor->iterator, nullptr, 0, subset};
			clast_filter(&(root->stmt), filter, &loops, &nloops, &stmts, &nstmts);
			free(stmts);
			for (int i = 0; i < nloops; ++i) {
				this->insertedStmts += 1;
				auto *s = pesto_clast_create_stmt(cloogOptions, nullptr, this->insertedStmts);
				clast_insert_indepth(&(loops[i]->stmt), &(s->stmt));
				it->innerBodyStmts0.push_back(s);
			}
			free(loops);
		}
		it->innerBodyStmt0 = it->innerBodyStmts0.empty() ? nullptr : it->innerBodyStmts0[0];
	}

	cloog_domain_free(enclosingDomain);
	if (innerEnclosingDomain)
		cloog_domain_free(innerEnclosingDomain);
}

void TpzTilingPass::codegen(Bundle &bundle, std::vector<pesto::pass::ptile::ClastTilingPair> &clastTilingPairs,
							isl_set *innerDomain, pesto::maths::Vec2i slope) {

	using pesto::ptile::NameHelper;

	auto &macroData = bundle.metadata.get<pesto::bundle::metadata::Macro>();
	auto &varDeclData = bundle.metadata.get<pesto::bundle::metadata::VarDecl>();

	macroData.nonProcessedList.push_back("#include <omp.h>");
	macroData.nonProcessedList.push_back("#include \"maxcut.h\"");

	{
		isl_printer *dp = isl_printer_to_str(this->ctx);

		// pour le commentaire gen_maxcut : domaine sans nom de tuple
		isl_set *domainForComment = isl_set_copy(innerDomain);
		domainForComment = isl_set_set_tuple_name(domainForComment, nullptr);
		dp = isl_printer_set_output_format(dp, ISL_FORMAT_ISL);
		dp = isl_printer_print_set(dp, domainForComment);
		isl_set_free(domainForComment);
		char *dstr = isl_printer_get_str(dp);
		std::string domainStr = dstr ? dstr : "<domain>";
		free(dstr);
		isl_printer_free(dp);

		std::string maxcutCmd = "/* max_cut_volume() comes from maxcut.h.\n"
								" * Regenerate it for THIS transformed domain (set --t to your DIV0 value):\n"
								"    python3 gen_maxcut.py \\\n"
								"      --domain \"" +
								domainStr +
								"\" \\\n"
								"      --t $DIV0 \\\n"
								"      --func max_cut_volume \\\n"
								"      --out maxcut.h\n"
								" * gen_maxcut computes min(x1) by lexmin; add --f1 <x_min> if it is piecewise.\n"
								" */";
		macroData.nonProcessedList.push_back(maxcutCmd);
	}

	// TODO: remove (debug)
	// isl_set_dump(innerDomain);

	// add var for front iterator
	std::string nthreadsName = std::string("NTHREADS");
	// std::string minVolName = std::string("D1_MIN_VOL");
	std::string offsetName = std::string("tile_offset");

	varDeclData.addVar(nthreadsName);
	varDeclData.addVar(offsetName);

	for (size_t i = 0; i < clastTilingPairs.size(); ++i) {
		auto &pair = clastTilingPairs[i];

		const char *outerItRaw = pair.outerFor ? pair.outerFor->iterator : nullptr;
		const char *innerItRaw = pair.innerFor ? pair.innerFor->iterator : nullptr;
		std::string outerItSafe = outerItRaw ? std::string(outerItRaw) : ("b" + std::to_string(pair.depth));
		std::string innerItSafe2 = innerItRaw ? std::string(innerItRaw) : ("z" + std::to_string(pair.depth));
		NameHelper nameHelper = NameHelper(pair.depth, outerItSafe.c_str(), innerItSafe2.c_str());

		varDeclData.addVar(nameHelper.pcMaxName());
		varDeclData.addVar(nameHelper.targetVolName());
		varDeclData.addVar(nameHelper.outerForLBName());
		varDeclData.addVar(nameHelper.outerForUBName());
		varDeclData.addVar(nameHelper.innerForLBName());
		varDeclData.addVar(nameHelper.innerForUBName());

		// compute bounds of the inner loop for the current loop
		isl_set *max_set = isl_set_copy(innerDomain);
		isl_set *min_set = isl_set_copy(innerDomain);
		for (int i = 0; i < isl_set_dim(innerDomain, isl_dim_set); ++i) {
			char const *dimNameRaw = isl_set_get_dim_name(innerDomain, isl_dim_set, i);
			std::string dimNameSafe =
				dimNameRaw ? std::string(dimNameRaw) : ("d" + std::to_string(pair.depth) + "_" + std::to_string(i));
			NameHelper helper = NameHelper(pair.depth, pair.outerFor->iterator, dimNameSafe.c_str());
			std::string lbName = helper.innerForLBName();
			std::string ubName = helper.innerForUBName();

			min_set = isl_set_set_dim_name(min_set, isl_dim_set, i, lbName.c_str());
			max_set = isl_set_set_dim_name(max_set, isl_dim_set, i, ubName.c_str());
		}
		std::string min_str = this->computeInnerMinMaxStr(min_set, pair.depth).first;

		std::string max_str = this->computeInnerMinMaxStr(max_set, pair.depth).second;

		if (pair.depth == 0) {
			// TODO: move to name helper
			std::string trapezoidalLBName = std::string("_tpzlb");
			std::string trapezoidalUBName = std::string("_tpzub");

			varDeclData.addVar(trapezoidalLBName);
			varDeclData.addVar(trapezoidalUBName);

			// The gap loop can have a stride (2 for diamond tiling): count bands in
			// effective iterations, so DIV0 means the same number of verticals as
			// on unit-stride kernels and no band spans empty ones.
			long strideVal = 1;
			if (pair.innerFor) {
				long s = mpz_get_si(pair.innerFor->stride);
				if (s > 0)
					strideVal = s;
			}
			std::string stride = std::to_string(strideVal);

			NameHelper nextHelper =
				NameHelper(1, clastTilingPairs[1].outerFor->iterator, clastTilingPairs[1].innerFor->iterator);

			std::string stmtStr = nameHelper.outerForLBName() + " = 0;";
			stmtStr += nameHelper.outerForUBName() + " = ";
			stmtStr += std::string() + "ceild((" + max_str + " - (" + min_str + ")) / " + stride + " + 1";
			stmtStr += ", " + nameHelper.divName() + ") - 1;";
			stmtStr += nthreadsName + " = omp_get_max_threads();";

			macroData.defineFunc("S" + std::to_string(pair.outerHeaderStmt->statement->number), "", stmtStr);

			varDeclData.addVar(nameHelper.outerForUBName());
			varDeclData.addVar(nameHelper.innerForLBName());
			varDeclData.addVar(nameHelper.innerForUBName());
			varDeclData.addVar(nameHelper.pcMaxName());

			stmtStr = nameHelper.innerForLBName() + " = " + pair.outerFor->iterator + " * " + nameHelper.divName() +
					  " * " + stride + " + " + min_str + ";";
			stmtStr += nameHelper.innerForUBName() + " = min((" + pair.outerFor->iterator + " + 1) * " +
					   nameHelper.divName() + " * " + stride + " - " + stride + " + " + min_str + ", " + max_str + ");";

			macroData.defineFunc("S" + std::to_string(pair.outerBodyStmt->statement->number), "", stmtStr);

			// Trapezoidal bounds
			auto d1Bounds = this->computeInnerMinMaxStr(innerDomain, 1);

			stmtStr = trapezoidalLBName + " = max(" + d1Bounds.first + ", " + nextHelper.innerForLBName() + " + " +
					  offsetName //
					  + " + floord(" + std::to_string(slope.y) + " * (" + pair.innerFor->iterator + " - " +
					  nameHelper.innerForLBName() + "), " + std::to_string(slope.x) + "));";
			stmtStr += trapezoidalUBName + " = min(" + d1Bounds.second + ", " + nextHelper.innerForUBName() + ");";

			for (auto *s : pair.innerBodyStmts0)
				macroData.defineFunc("S" + std::to_string(s->statement->number), "", stmtStr);

			stmtStr = "";
			stmtStr += trapezoidalLBName + " = max(" + d1Bounds.first + ", " + nextHelper.innerForLBName() + ");";
			stmtStr += trapezoidalUBName + " = min(min(" + d1Bounds.second + ", " + nextHelper.innerForUBName() +
					   "), " + nextHelper.innerForLBName() + " + " + offsetName //
					   + " + floord(" + std::to_string(slope.y) + " * (" + pair.innerFor->iterator + " - " +
					   nameHelper.innerForLBName() + "), " + std::to_string(slope.x) + ") - 1);";

			for (auto *s : pair.innerBodyStmts1) {
				macroData.defineFunc("S" + std::to_string(s->statement->number), "", stmtStr);
			}
		}

		if (pair.depth == 1) {

			// header (out of tiling loop) stmt
			std::string stmtStr = nameHelper.pcMaxName() + " = " + nameHelper.ehrhartFnName() + "(";
			// add all the parameters
			isl_size npar = isl_set_dim(innerDomain, isl_dim_param);
			for (isl_size i = 0; i < npar; ++i) {
				stmtStr += isl_set_get_dim_name(innerDomain, isl_dim_param, i);
				stmtStr += ", ";
			}
			NameHelper rootHelper =
				NameHelper(0, clastTilingPairs[0].outerFor->iterator, clastTilingPairs[0].innerFor->iterator);
			stmtStr += rootHelper.innerForLBName() + ", " + rootHelper.innerForUBName();
			stmtStr += ");";

			stmtStr += nameHelper.outerForLBName() + " = 0;";

			/* if (this->useMinVolume) {
				stmtStr +=
					std::string("long int NB = max(floord(") + nameHelper.pcMaxName() + ", " + minVolName + "), 1);";

				stmtStr += std::string("if(NB < ") + nthreadsName + ") {";
				stmtStr += nameHelper.outerForUBName() + " = NB - 1;";
				stmtStr += nameHelper.targetVolName() + " = floord(" + nameHelper.pcMaxName() + ", NB);";
				stmtStr += "} else {";
				stmtStr += nameHelper.outerForUBName() + " = NB - (NB % " + nthreadsName + ") - 1;";
				stmtStr += nameHelper.targetVolName() + " = floord(" + nameHelper.pcMaxName() + ", " +
						   nameHelper.outerForUBName() + " + 1);";
				stmtStr += "}";
			} else { */
			std::string bandIter = clastTilingPairs[0].outerFor->iterator;
			stmtStr += nameHelper.targetVolName() + " = max(" + nameHelper.pcMaxName() + " / " + nameHelper.divName() +
					   ", max_cut_volume(" + bandIter + "));";
			stmtStr += nameHelper.outerForUBName() + " = max(floord(" + nameHelper.pcMaxName() + ", " +
					   nameHelper.targetVolName() + "), 1) - 1;";
			//}

			macroData.defineFunc(std::string("S") + std::to_string(pair.outerHeaderStmt->statement->number), "",
								 stmtStr);

			// body stmt (inside the tiling for)

			// compute bounds using trahrhe functions
			// lower bound
			stmtStr = nameHelper.innerForLBName() + " = " + nameHelper.trahrheFnName() + "(";
			stmtStr += std::string("max(") + pair.outerFor->iterator + " * " + nameHelper.targetVolName() + ", 1)";
			for (isl_size i = 0; i < npar; ++i) {
				stmtStr += ", ";
				stmtStr += isl_set_get_dim_name(innerDomain, isl_dim_param, i);
			}
			stmtStr += "," + rootHelper.innerForLBName() + ", " + rootHelper.innerForUBName();
			stmtStr += ");";
			// outer bound
			stmtStr += nameHelper.innerForUBName() + " = " + nameHelper.trahrheFnName() + "(";
			stmtStr += std::string("min((") + pair.outerFor->iterator + " + 1) * " + nameHelper.targetVolName() + ", " +
					   nameHelper.pcMaxName() + ")";

			for (isl_size i = 0; i < npar; ++i) {
				stmtStr += ", ";
				stmtStr += isl_set_get_dim_name(innerDomain, isl_dim_param, i);
			}
			stmtStr += "," + rootHelper.innerForLBName() + ", " + rootHelper.innerForUBName();
			stmtStr += ") - 1;";

			// last tile bound correction (to cover entire domain)
			stmtStr += std::string("") + "if(" + pair.outerFor->iterator + " == " + nameHelper.outerForUBName() + ") {";
			stmtStr += nameHelper.innerForUBName() + " = " + max_str + ";";
			stmtStr += "}" + std::string("");

			// band is WIDE (in x), otherwise the diagonal cut cannot span the
			// band. Using the actual band width is correct for any cone slope;
			// the previous floord(slope.y*DIV, slope.x) degenerated to 0 when
			// slope.y was 0 (e.g. nussinov), disabling the check.

			// The band width must be counted in effective iterations: the gap
			// loop can have a stride (2 for diamond tiling), so ubt1 - lbt1 + 1
			// over-counts and the check demands more than the band holds.
			long rootStrideVal = 1;
			if (clastTilingPairs[0].innerFor) {
				long s = mpz_get_si(clastTilingPairs[0].innerFor->stride);
				if (s > 0)
					rootStrideVal = s;
			}
			std::string stride = std::to_string(rootStrideVal);

			stmtStr += "if(" + nameHelper.innerForUBName() + " - " + nameHelper.innerForLBName() + " + 1 < ((" +
					   rootHelper.innerForUBName() + " - " + rootHelper.innerForLBName() + ") / " + stride +
					   " + 1) && (" + pair.outerFor->iterator + " != " + nameHelper.outerForUBName() + ") && (" +
					   nameHelper.outerForUBName() + " > 1)) {";
			stmtStr += "fprintf(stderr, \"Error: Target volume too small.\\n(" + //
					   nameHelper.targetVolName() + ", " +						 // target volume
					   rootHelper.innerForLBName() + ", " +						 // d0 inner lower bound
					   rootHelper.innerForUBName() + ", " +						 // d0 inner upper bound
					   nameHelper.innerForLBName() + ", " +						 // d1 inner lower bound
					   nameHelper.innerForUBName() +							 // d1 inner upper bound
					   ") = (%ld, %ld, %ld, %ld, %ld)\\n\", " +					 //
					   nameHelper.targetVolName() + ", " +						 // target volume
					   rootHelper.innerForLBName() + ", " +						 // d0 inner lower bound
					   rootHelper.innerForUBName() + ", " +						 // d0 inner upper bound
					   nameHelper.innerForLBName() + ", " +						 // d1 inner lower bound
					   nameHelper.innerForUBName() +							 // d1 inner upper bound
					   ");";
			stmtStr += "exit(1);";
			stmtStr += "}" + std::string("");

			// inner tile offset
			stmtStr += offsetName + " = max(";
			stmtStr += "floord((" + nameHelper.innerForUBName() + " - " + nameHelper.innerForLBName() + ") - " +
					   "DIV0" + ", 2)";
			stmtStr += ", 0);";

			macroData.defineFunc(std::string("S") + std::to_string(pair.outerBodyStmt->statement->number), "", stmtStr);

			for (auto *s : pair.innerBodyStmts0)
				macroData.defineFunc("S" + std::to_string(s->statement->number), "", "");
			for (auto *s : pair.innerBodyStmts1)
				macroData.defineFunc("S" + std::to_string(s->statement->number), "", "");
		}

		// dans la boucle, avant if(pair.underGap) :
		logger->debug("PAIR depth={} underGap={}", pair.depth, pair.underGap);

		// for depth > 1 we do rectangular tiling
		if (pair.depth > 1) {
			if (pair.underGap) {
				// lbzt6 / ubzt6 : posés juste avant la boucle zt6, donc dans le
				// corps de t2, après les bornes trapézoïdales dont ils dépendent.
				std::string stmtStr = nameHelper.outerForLBName() + " = 0;";
				stmtStr += nameHelper.outerForUBName() + " = ceild((_tpzub - 1) - (_tpzlb - " +
						   clastTilingPairs[0].innerFor->iterator + ") + 1, " + nameHelper.divName() + ") - 1;";
				// One tile loop per reduction nest (lu's two point loops): each
				// gets its own bound statements so it recomputes lbt3/ubt3.
				for (auto *h : pair.outerHeaderStmts)
					macroData.defineFunc("S" + std::to_string(h->statement->number), "", stmtStr);
				varDeclData.addVar(nameHelper.outerForLBName());
				varDeclData.addVar(nameHelper.outerForUBName());

				// corps de zt6 : bornes de la tuile de réduction, et restriction de
				// t4 aux valeurs dont la plage de réduction rencontre cette tuile.
				stmtStr = nameHelper.innerForLBName() + " = " + pair.outerFor->iterator + " * " + nameHelper.divName() +
						  " + (_tpzlb - " + clastTilingPairs[0].innerFor->iterator + ");";
				stmtStr += nameHelper.innerForUBName() + " = min((" + pair.outerFor->iterator + " + 1) * " +
						   nameHelper.divName() + " - 1 + (_tpzlb - " + clastTilingPairs[0].innerFor->iterator +
						   "), _tpzub - 1);";
				stmtStr += "_tpzlb2 = max(_tpzlb, " + nameHelper.innerForLBName() + " + 1);";
				stmtStr += "_tpzub2 = min(_tpzub, " + nameHelper.innerForUBName() + " + " +
						   clastTilingPairs[0].innerFor->iterator + ");";
				for (auto *b : pair.outerBodyStmts)
					macroData.defineFunc("S" + std::to_string(b->statement->number), "", stmtStr);
				varDeclData.addVar(nameHelper.innerForLBName());
				varDeclData.addVar(nameHelper.innerForUBName());
				varDeclData.addVar("_tpzlb2");
				varDeclData.addVar("_tpzub2");

				for (auto *s : pair.innerBodyStmts0)
					macroData.defineFunc("S" + std::to_string(s->statement->number), "", "");
				for (auto *s : pair.innerBodyStmts1)
					macroData.defineFunc("S" + std::to_string(s->statement->number), "", "");
			} else {
				std::string stmtStr = "";
				stmtStr += nameHelper.outerForLBName() + " = 0;";
				stmtStr += nameHelper.outerForUBName() + " = ceild(" + max_str + " - (" + min_str + "), " +
						   nameHelper.divName() + ");";
				macroData.defineFunc("S" + std::to_string(pair.outerHeaderStmt->statement->number), "", stmtStr);
				stmtStr = nameHelper.innerForLBName() + " = " + pair.outerFor->iterator + " * " + nameHelper.divName() +
						  " + " + min_str + ";";
				stmtStr += nameHelper.innerForUBName() + " = min((" + pair.outerFor->iterator + " + 1) * " +
						   nameHelper.divName() + " + " + min_str + " - 1, " + max_str + ");";
				macroData.defineFunc("S" + std::to_string(pair.outerBodyStmt->statement->number), "", stmtStr);
				for (auto *s : pair.innerBodyStmts0)
					macroData.defineFunc("S" + std::to_string(s->statement->number), "", "");
				for (auto *s : pair.innerBodyStmts1)
					macroData.defineFunc("S" + std::to_string(s->statement->number), "", "");
			}
		}
		isl_set_free(max_set);
		isl_set_free(min_set);
	}
}

__isl_give isl_set *TpzTilingPass::computeInnerDomain(pesto::ir::ClastIR &clastIR,
													  std::vector<ptile::ClastTilingPair> &clastTilingPairs) {

	CloogProgram *cloogProgram = clastIR.getCloogProgram();
	struct clast_for *rootFor = clastTilingPairs[0].innerFor;
	int nstmts = 0;
	clast_user_stmt **userStmts = clast_get_user_stmt_list(&(rootFor->stmt), &nstmts);

	logger->debug("Found {} user statements in the inner for loop: {}", nstmts, clastTilingPairs[0].innerFor->iterator);

	isl_union_set *domain = nullptr;
	for (int i = 0; i < nstmts; ++i) {
		clast_user_stmt *userStmt = userStmts[i];
		struct clast_for *parentFor = clast_get_parent_clast_for(&(rootFor->stmt), &(userStmt->stmt));

		assert(parentFor != nullptr && "Parent for loop should not be null");

		logger->debug("Processing user statement: {} in parent for loop: {} ({})", userStmt->statement->number,
					  parentFor->iterator, (uintptr_t)parentFor);

		// isl_set_dump(isl_set_from_cloog_domain(parentFor->domain));

		if (domain == nullptr) {
			domain = isl_union_set_from_set(isl_set_copy(isl_set_from_cloog_domain(parentFor->domain)));
		} else {
			domain = isl_union_set_add_set(domain, isl_set_copy(isl_set_from_cloog_domain(parentFor->domain)));
		}
	}

	free(userStmts);

	// Multi-statement scops give a union of domains with different
	// dimensionality (e.g. nussinov: a 3-D statement inside the k-loop and a
	// 2-D one outside). isl_set_from_union_set requires one space and crashes.
	// The trapezoidal (x,y,z) domain is the one of the highest-dimensional
	// statement (the reduction nest); take that one rather than the union.
	// The nest must cover every statement, not just the deepest one. Cholesky's
	// finalisations live at t2 = 0, where the reduction is empty; keeping only the
	// deepest domain inherits its 0 <= t3 < t2, so the gap starts at t1 = 3 and
	// column j = 0 is never generated. Pad the shallower domains with the missing
	// trailing dims fixed to 0 and union everything instead.
	isl_set *set = nullptr;
	{
		struct Dims {
			int max;
			int min;
		} d{0, INT_MAX};
		isl_union_set_foreach_set(
			domain,
			+[](__isl_take isl_set *s, void *user) -> isl_stat {
				auto *d = static_cast<Dims *>(user);
				int nd = isl_set_dim(s, isl_dim_set);
				if (nd > d->max)
					d->max = nd;
				if (nd < d->min)
					d->min = nd;
				isl_set_free(s);
				return isl_stat_ok;
			},
			&d);
		const int maxDim = d.max;

		// The projection and the re-added dimension exist to reconcile statements
		// of differing depth: cholesky's and lu's finalisations live where the
		// reduction is empty. With a uniform depth — seidel-2d, 3d7pt — every
		// dimension keeps its own bounds and nothing needs re-adding.
		const int nRed = (d.max > d.min) ? 1 : 0;
		const int nOuter = maxDim - nRed;
		// Union of every statement's domain projected onto the outer dims: the
		// nest must cover all of them, including the finalisations that live at
		// t2 = 0 where the reduction is empty.
		struct Acc {
			isl_set *acc;
			int nOuter;
		} a{nullptr, nOuter};
		isl_union_set_foreach_set(
			domain,
			+[](__isl_take isl_set *s, void *user) -> isl_stat {
				auto *a = static_cast<Acc *>(user);
				int nd = isl_set_dim(s, isl_dim_set);
				// Shallower statements are emitted above this nest; padding the
				// missing dims with 0 anchors them at a point outside the domain
				// and makes the hull invent skewed parametric facets.
				if (nd < a->nOuter) {
					isl_set_free(s);
					return isl_stat_ok;
				}
				s = isl_set_set_tuple_name(s, nullptr);
				if (nd > a->nOuter)
					s = isl_set_project_out(s, isl_dim_set, a->nOuter, nd - a->nOuter);
				a->acc = a->acc ? isl_set_union(a->acc, s) : s;
				return isl_stat_ok;
			},
			&a);
		isl_union_set_free(domain);

		// Hull on the outer dims only: hulling padded domains of different
		// dimensionality invents skewed parametric facets Trahrhe cannot parse.
		isl_set *outer = isl_set_coalesce(a.acc);
		if (isl_set_n_basic_set(outer) != 1) {
			logger->debug("outer domain not convex after coalesce, falling back to hull");
			outer = isl_set_from_basic_set(isl_set_convex_hull(outer));
		}

		// Re-add the reduction dim with its own bounds: 0 <= t_red <= t_{nOuter-1}.
		set = outer;
		if (nRed > 0) {
			// Re-add the reduction dim with its own bounds: 0 <= t_red <= t_{nOuter-1}.
			set = isl_set_add_dims(set, isl_dim_set, nRed);
			isl_local_space *ls = isl_local_space_from_space(isl_set_get_space(set));
			isl_constraint *lo = isl_constraint_alloc_inequality(isl_local_space_copy(ls));
			lo = isl_constraint_set_coefficient_si(lo, isl_dim_set, nOuter, 1);
			set = isl_set_add_constraint(set, lo);
			isl_constraint *hi = isl_constraint_alloc_inequality(ls);
			hi = isl_constraint_set_coefficient_si(hi, isl_dim_set, nOuter, -1);
			hi = isl_constraint_set_coefficient_si(hi, isl_dim_set, nOuter - 1, 1);
			set = isl_set_add_constraint(set, hi);
		}
	}

	set = isl_set_coalesce(set);
	if (isl_set_n_basic_set(set) != 1) {
		logger->debug("inner domain not convex after coalesce, falling back to hull");
		set = isl_set_from_basic_set(isl_set_convex_hull(set));
	}

	for (size_t i = 0; i < clastTilingPairs.size(); ++i)
		logger->debug("clastTilingPair[{}] innerFor iterator: {}", i, clastTilingPairs[i].innerFor->iterator);

	// The detected-stencil schedule (from Pluto's size-1 tiling) can leave a
	// dimension equal to another (t4 = t2), giving the inner domain one more
	// dimension than there are tiling levels. Find an equality of the form
	// dim_hi - dim_lo = 0 and project out dim_hi (the redundant one), until the
	// domain has one dimension per tiling level. DP kernels have no such
	// equality, so the loop never fires and they are unchanged.
	{
		// The detected-stencil schedule (from Pluto's size-1 tiling) can leave a
		// dimension equal to another (t4 = t2), giving the inner domain one more
		// dimension than there are tiling levels. Find an equality of the form
		// dim_hi - dim_lo = 0 and project out dim_hi (the redundant one), until the
		// domain has one dimension per tiling level. DP kernels have no such
		// equality, so the loop never fires and they are unchanged.
		{
			int nlevels = (int)clastTilingPairs.size();
			bool changed = true;
			while (changed && isl_set_dim(set, isl_dim_set) > nlevels) {
				changed = false;
				isl_basic_set *aff = isl_set_affine_hull(isl_set_copy(set));

				struct DegFinder {
					int nd;
					int hi;
				} df{isl_set_dim(set, isl_dim_set), -1};
				isl_basic_set_foreach_constraint(
					aff,
					+[](__isl_take isl_constraint *con, void *user) -> isl_stat {
						auto *df = static_cast<DegFinder *>(user);
						if (df->hi >= 0) {
							isl_constraint_free(con);
							return isl_stat_ok;
						}
						if (isl_constraint_is_equality(con) == isl_bool_true) {
							int hi = -1, lo = -1, nonzero = 0;
							bool clean = true;
							for (int d = 0; d < df->nd; ++d) {
								isl_val *v = isl_constraint_get_coefficient_val(con, isl_dim_set, d);
								long vi = isl_val_get_num_si(v);
								isl_val_free(v);
								if (vi == 0)
									continue;
								nonzero++;
								if (vi == 1 || vi == -1) {
									if (d > hi) {
										lo = hi;
										hi = d;
									} else if (d > lo)
										lo = d;
								} else
									clean = false;
							}
							isl_val *cst = isl_constraint_get_constant_val(con);
							long csti = isl_val_get_num_si(cst);
							isl_val_free(cst);
							if (clean && nonzero == 2 && hi >= 0 && lo >= 0 && csti == 0)
								df->hi = hi;
						}
						isl_constraint_free(con);
						return isl_stat_ok;
					},
					&df);

				isl_basic_set_free(aff);
				if (df.hi >= 0) {
					logger->debug("projecting degenerate dim {}", df.hi);
					set = isl_set_project_out(set, isl_dim_set, df.hi, 1);
					changed = true;
				}
			}
		}
	}

	logger->debug("innerDomain dims: {} tiling levels: {}", isl_set_dim(set, isl_dim_set), clastTilingPairs.size());

	for (size_t i = 0; i < clastTilingPairs.size(); ++i) {
		ptile::ClastTilingPair &pair = clastTilingPairs[i];
		set = isl_set_set_dim_name(set, isl_dim_set, i, pair.innerFor->iterator);
	}
	// Dimensions beyond the tiling levels (an untiled reduction) keep no name,
	// and Trahrhe writes names with fputs — a null one segfaults.
	for (int i = 0; i < isl_set_dim(set, isl_dim_set); ++i)
		if (!isl_set_has_dim_name(set, isl_dim_set, i)) {
			std::string nm = "d" + std::to_string(i);
			set = isl_set_set_dim_name(set, isl_dim_set, i, nm.c_str());
		}

	for (int i = 0; i < cloogProgram->names->nb_parameters; i += 1) {
		set = isl_set_set_dim_name(set, isl_dim_param, i, cloogProgram->names->parameters[i]);
	}

	return set;
}

void TpzTilingPass::generateTrahrheHeaders(pesto::Bundle &bundle, isl_set *innerDomain) {
	auto &macroData = bundle.metadata.get<pesto::bundle::metadata::Macro>();

	std::filesystem::path outputDir = ".";
	std::string filename = "header.";

	if (bundle.metadata.has<pesto::bundle::metadata::FileInfo>()) {
		auto &fileinfo = bundle.metadata.get<pesto::bundle::metadata::FileInfo>();
		filename = fileinfo.output.stem().string() + ".";
		if (fileinfo.output.has_parent_path()) {
			outputDir = fileinfo.output.parent_path();
		}
	}

	std::string header_filename = filename + "trahrhe.n" + std::to_string(0) + ".h";

	trahrhe_context_t *trahrhe_ctx = trahrhe_context_alloc();
	trahrhe_context_use_dichotomy(trahrhe_ctx, true);
	trahrhe_context_set_verbosity(trahrhe_ctx, TRAHRHE_VERBOSITY_INFO);

	trahrhe_context_set_input_domain(trahrhe_ctx, isl_set_copy(innerDomain));
	trahrhe_context_set_tile_level(trahrhe_ctx, 2);

	size_t trahrhe_len = 0;
	char **trahrhe_expr = trahrhe_get_inverted_polynomials_expr(trahrhe_ctx, &trahrhe_len);
	if (trahrhe_len == 0 || trahrhe_expr == NULL) {
		logger->error("Error: Could not compute trahrhe expr");
		trahrhe_context_free(trahrhe_ctx);
		return;
	}

	std::string header_path = (outputDir / header_filename).string();
	FILE *fp = fopen(header_path.c_str(), "w");
	if (fp == NULL) {
		logger->error("Error: Could not open file {}", header_filename.c_str());
		trahrhe_context_free(trahrhe_ctx);
		goto error;
	}

	logger->info("Generating trahrhe header: {}", header_filename.c_str());

	trahrhe_generate_header(trahrhe_ctx, fp, trahrhe_len, trahrhe_expr);

	fclose(fp);

	macroData.nonProcessedList.push_back("#include \"" + header_filename + "\"");

	trahrhe_context_free(trahrhe_ctx);
error:
	if (trahrhe_expr != NULL) {
		for (size_t i = 0; i < trahrhe_len; i++) {
			if (trahrhe_expr[i] != NULL) {
				free(trahrhe_expr[i]);
			}
		}
	}
	free(trahrhe_expr);
}

void TpzTilingPass::insertInnerLoopBounds(pesto::ir::ClastIR &clastIR,
										  std::vector<ptile::ClastTilingPair> &clastTilingPairs) {
	using pesto::ptile::NameHelper;

	int nclastloops, nclaststmts;
	struct clast_for **clastloops = NULL;
	int *claststmts = NULL;

	struct clast_for *root = clastTilingPairs[0].outerFor;

	for (auto &pair : clastTilingPairs) {
		if (pair.innerFor == nullptr) {
			continue;
		}
		ClastFilter filter = {pair.innerFor->iterator, nullptr, 0, subset};

		clast_filter(&(root->stmt), filter, &clastloops, &nclastloops, &claststmts, &nclaststmts);
		free(claststmts);

		NameHelper nameHelper = NameHelper(pair.depth, pair.outerFor->iterator, pair.innerFor->iterator);

		// std::string lbName = nameHelper.innerForLBName();
		// std::string ubName = nameHelper.innerForUBName();

		for (int i = 0; i < nclastloops; ++i) {
			struct clast_for *clastFor = clastloops[i];

			std::string lbName = nameHelper.innerForLBName();
			std::string ubName = nameHelper.innerForUBName();
			if (pair.depth == 1) {
				// The reduction level can hold several tile loops (one per nest);
				// the point loop takes the tile-restricted bounds if any of them
				// is its parent.
				bool underRedTile = false;
				if (clastTilingPairs.size() == 3 && clastTilingPairs[2].underGap) {
					struct clast_for *p = clast_get_parent_clast_for(&(root->stmt), &(clastFor->stmt));
					for (auto *of : clastTilingPairs[2].outerFors)
						if (p == of) {
							underRedTile = true;
							break;
						}
					if (clastTilingPairs[2].outerFors.empty())
						underRedTile = (p == clastTilingPairs[2].outerFor);
				}
				lbName = underRedTile ? "_tpzlb2" : "_tpzlb";
				ubName = underRedTile ? "_tpzub2" : "_tpzub";
			}

			std::unique_ptr<std::string> lbPtr = std::make_unique<std::string>(lbName);
			std::unique_ptr<std::string> ubPtr = std::make_unique<std::string>(ubName);

			struct clast_name *paramLB = new_clast_name(lbPtr->c_str());
			struct clast_name *paramUB = new_clast_name(ubPtr->c_str());

			struct clast_reduction *lbRed = new_clast_reduction(clast_red_max, 2);
			lbRed->elts[0] = &(paramLB->expr);
			lbRed->elts[1] = clastFor->LB;
			struct clast_reduction *ubRed = new_clast_reduction(clast_red_min, 2);
			ubRed->elts[0] = &(paramUB->expr);
			ubRed->elts[1] = clastFor->UB;
			clastFor->LB = &(lbRed->expr);
			clastFor->UB = &(ubRed->expr);

			this->stringKeepAlive.push_back(std::move(lbPtr));
			this->stringKeepAlive.push_back(std::move(ubPtr));
		}

		free(clastloops);
	}
}

void TpzTilingPass::insertOmpPragmas(pesto::ir::ClastIR &clastIR,
									 std::vector<ptile::ClastTilingPair> &clastTilingPairs) {
	using pesto::ptile::NameHelper;

	(void)clastIR; // Unused parameter

	int nclastloops, nclaststmts;
	struct clast_for **clastloops = NULL;
	int *claststmts = NULL;

	struct clast_for *root = clastTilingPairs[0].outerFor;

	// remove all omp pragmas from the inner loops
	for (auto &pair : clastTilingPairs) {
		if (pair.innerFor == nullptr) {
			continue;
		}
		ClastFilter filter = {pair.innerFor->iterator, nullptr, 0, subset};

		clast_filter(&(root->stmt), filter, &clastloops, &nclastloops, &claststmts, &nclaststmts);
		free(claststmts);

		NameHelper nameHelper = NameHelper(pair.depth, pair.outerFor->iterator, pair.innerFor->iterator);

		for (int i = 0; i < nclastloops; ++i) {
			struct clast_for *clastFor = clastloops[i];
			clastFor->parallel = CLAST_PARALLEL_NOT; // Disable parallelism for the inner loops
		}

		free(clastloops);
	}

	std::string userDirective = "omp parallel for firstprivate(";

	{
		NameHelper nameHelper0 =
			NameHelper(0, clastTilingPairs[0].outerFor->iterator, clastTilingPairs[0].innerFor->iterator);
		NameHelper nameHelper1 =
			NameHelper(1, clastTilingPairs[1].outerFor->iterator, clastTilingPairs[1].innerFor->iterator);
		userDirective += "lbp, ubp, ";
		userDirective += nameHelper0.outerForLBName() + ", " + nameHelper0.outerForUBName() + ", ";
		userDirective += nameHelper1.outerForLBName() + ", " + nameHelper1.outerForUBName() + ", ";
		userDirective += nameHelper0.innerForLBName() + ", " + nameHelper0.innerForUBName() + ", ";
		userDirective += nameHelper1.pcMaxName() + ", " + nameHelper1.targetVolName();

		userDirective += ") ";

		userDirective += "num_threads(min(NTHREADS, " + nameHelper1.outerForUBName() + " + 1 )) ";
	}

	std::string privateVars = "";

	// add inner for iterators
	for (auto &pair : clastTilingPairs) {
		if (!privateVars.empty()) {
			privateVars += ", ";
		}
		privateVars += pair.innerFor->iterator;
	}

	// CLooG can leave loops below the tiled levels that have no tiling pair —
	// an untiled reduction, or a deeper sibling nest. Their iterators would
	// stay shared between threads.
	{
		std::vector<std::string> extra;
		std::function<void(struct clast_stmt *)> collect = [&](struct clast_stmt *s) {
			for (; s != NULL; s = s->next) {
				if (CLAST_STMT_IS_A(s, stmt_for)) {
					struct clast_for *f = (struct clast_for *)s;
					std::string it = f->iterator;
					bool known = false;
					for (auto &p : clastTilingPairs)
						if (it == p.innerFor->iterator || it == p.outerFor->iterator) {
							known = true;
							break;
						}
					for (auto &e : extra)
						if (it == e) {
							known = true;
							break;
						}
					if (!known)
						extra.push_back(it);
					collect(f->body);
				} else if (CLAST_STMT_IS_A(s, stmt_guard)) {
					collect(((struct clast_guard *)s)->then);
				} else if (CLAST_STMT_IS_A(s, stmt_block)) {
					collect(((struct clast_block *)s)->body);
				}
			}
		};
		collect(root->body);
		for (auto &e : extra)
			privateVars += ", " + e;
		for (auto &e : extra)
			if (privateVars.find(e) == std::string::npos)
				privateVars += ", " + e;
	}

	// add outer for iterators if depth > 1
	for (auto &pair : clastTilingPairs) {
		if (pair.depth < 2) {
			continue;
		}
		if (!privateVars.empty()) {
			privateVars += ", ";
		}
		NameHelper nameHelper = NameHelper(pair.depth, pair.outerFor->iterator, pair.innerFor->iterator);

		privateVars += pair.outerFor->iterator;
		privateVars += "," + nameHelper.outerForLBName();
		privateVars += ", " + nameHelper.outerForUBName();
	}

	// add inner for bounds
	for (auto &pair : clastTilingPairs) {
		if (pair.depth < 1) {
			continue;
		}
		NameHelper nameHelper = NameHelper(pair.depth, pair.outerFor->iterator, pair.innerFor->iterator);
		if (!privateVars.empty()) {
			privateVars += ", ";
		}
		privateVars += nameHelper.innerForLBName() + ", " + nameHelper.innerForUBName();
	}
	privateVars += std::string(", _tpzlb") + ", _tpzub" + ", tile_offset";
	if (clastTilingPairs.size() == 3 && clastTilingPairs[2].underGap)
		privateVars += ", _tpzlb2, _tpzub2";

	// Scalars written in the region are not indexed by the point: without this,
	// every slice shares one location (symm's temp2).
	for (const auto &nm : this->scalarWrites_)
		if (privateVars.find(nm) == std::string::npos)
			privateVars += ", " + nm;

	// add omp pragmas to the outer loops of depth 1
	ClastFilter filter = {clastTilingPairs[1].outerFor->iterator, nullptr, 0, subset};

	clast_filter(&(root->stmt), filter, &clastloops, &nclastloops, &claststmts, &nclaststmts);
	free(claststmts);

	for (int i = 0; i < nclastloops; ++i) {
		struct clast_for *clastFor = clastloops[i];

		clastFor->parallel = CLAST_PARALLEL_OMP | CLAST_PARALLEL_USER; // Disable parallelism for the inner loops
		clastFor->private_vars = strdup(privateVars.c_str());
		clastFor->user_directive = strdup(userDirective.c_str());
	}

	free(clastloops);
}

std::optional<pesto::maths::Vec2i> TpzTilingPass::computeDepDistanceVector(osl_scop_p scop) {
	using pesto::ir::converter::Scop2Isl;

	// osl_scop_print(stdout, scop);

	Scop2Isl irHelper(this->ctx);
	irHelper.convert(scop);

	isl_union_set *domains = irHelper.getDomains();
	isl_union_map *schedules = irHelper.getSchedules();
	isl_union_map *writes = irHelper.getWrites();
	isl_union_map *mayWrites = irHelper.getMayWrites();
	isl_union_map *reads = irHelper.getReads();
	isl_union_map *empty = isl_union_map_empty(irHelper.getParamSpace());

	isl_union_access_info *access;
	isl_union_flow *flow;

	isl_union_map *dep_flow; // potential flow dependences

	isl_map_list *schedule_list = isl_union_map_get_map_list(schedules);
	isl_union_map *named_schedules = isl_union_map_empty_ctx(this->ctx);

	for (isl_size i = 0; i < isl_map_list_n_map(schedule_list); ++i) {
		isl_map *map = isl_map_list_get_at(schedule_list, i);

		map = isl_map_set_tuple_name(map, isl_dim_out, isl_map_get_tuple_name(map, isl_dim_in));
		named_schedules = isl_union_map_add_map(named_schedules, map);
	}
	isl_map_list_free(schedule_list);

	// writes = isl_union_map_apply_domain(writes, isl_union_map_copy(named_schedules));
	// reads = isl_union_map_apply_domain(reads, isl_union_map_copy(named_schedules));

	// This is for debugging purposes
	/* std::cerr << "Domains: " << std::endl;
	 isl_union_set_dump(domains);
	 std::cerr << "Schedule" << std::endl;
	 isl_union_map_dump(schedules);
	 std::cerr << "Named Schedules" << std::endl;
	 isl_union_map_dump(named_schedules);
	 std::cerr << "Writes: " << std::endl;
	 isl_union_map_dump(writes);
	 std::cerr << "Reads: " << std::endl;
	 isl_union_map_dump(reads); */

	// Dep := (last W before R under S);

	// compute live in
	access = isl_union_access_info_from_sink(isl_union_map_copy(reads));
	access = isl_union_access_info_set_must_source(access, isl_union_map_copy(writes));
	access = isl_union_access_info_set_schedule_map(access, isl_union_map_copy(schedules));
	flow = isl_union_access_info_compute_flow(access);
	dep_flow = isl_union_flow_get_must_dependence(flow);
	isl_union_flow_free(flow);

	// DEBUG
	// std::cerr << "Dep Flow: " << std::endl;
	// isl_union_map_dump(dep_flow);
	dep_flow = isl_union_map_apply_range(dep_flow, isl_union_map_copy(named_schedules));
	dep_flow = isl_union_map_apply_domain(dep_flow, isl_union_map_copy(named_schedules));

	isl_union_map *sanitized_dep_flow = nullptr;
	isl_map_list *list = isl_union_map_get_map_list(dep_flow);
	for (int i = 0; i < isl_map_list_n_map(list); ++i) {
		isl_map *map = isl_map_list_get_map(list, i);

		map = isl_map_set_tuple_name(map, isl_dim_in, nullptr);
		map = isl_map_set_tuple_name(map, isl_dim_out, nullptr);

		if (sanitized_dep_flow == nullptr) {
			sanitized_dep_flow = isl_union_map_from_map(isl_map_copy(map));
		} else {
			sanitized_dep_flow = isl_union_map_add_map(sanitized_dep_flow, isl_map_copy(map));
		}

		isl_map_free(map);
	}
	isl_map_list_free(list);

	isl_union_set *deltas = isl_union_map_deltas(isl_union_map_copy(sanitized_dep_flow));

	bool canonical = isCanonicalLayout(deltas); // ← ajouté
	logger->debug("delta layout: {}", canonical ? "canonical" : "raw pluto");

	struct DeltaProj {
		isl_ctx *ctx;
		isl_set *acc;
		bool canonical;					 // ← ajouté
	} dp{this->ctx, nullptr, canonical}; // ← 3e champ

	isl_union_set_foreach_set(
		deltas,
		+[](__isl_take isl_set *d, void *user) -> isl_stat {
			auto *dp = static_cast<DeltaProj *>(user);
			int nd = isl_set_dim(d, isl_dim_set);
			if (dp->canonical) {
				// [0,x,0,y,0,z,0] : axes de boucle en 1 et 3
				if (nd < 4) {
					isl_set_free(d);
					return isl_stat_ok;
				}
				d = isl_set_project_out(d, isl_dim_set, 4, nd - 4);
				d = isl_set_project_out(d, isl_dim_set, 2, 1);
				d = isl_set_project_out(d, isl_dim_set, 0, 1);
			} else {
				// schedule brut de Pluto : gap en 0, première dim d'espace en 1
				if (nd < 2) {
					isl_set_free(d);
					return isl_stat_ok;
				}
				if (nd > 2)
					d = isl_set_project_out(d, isl_dim_set, 2, nd - 2);
			}
			d = isl_set_set_tuple_name(d, nullptr);
			dp->acc = dp->acc ? isl_set_union(dp->acc, d) : d;
			return isl_stat_ok;
		},
		&dp);

	isl_set *set = dp.acc ? dp.acc : isl_set_empty(isl_space_set_alloc(this->ctx, 0, 2));
	set = isl_set_coalesce(set);

	isl_map *proj = isl_set_identity(isl_set_copy(set));
	if (isl_map_dim(proj, isl_dim_out) > 2) {
		proj = isl_map_remove_dims(proj, isl_dim_out, 2, isl_map_dim(proj, isl_dim_out) - 2);
	}

	set = isl_set_apply(set, proj);
	// Project the parameters out instead of dropping the constraints that
	// mention them: dropping is an over-approximation that loses the sign
	// constraints when a dependence distance is bounded by a parameter
	// (d > l here), which made the diagonal family symmetric and produced a
	// spurious negative vector.
	set = isl_set_project_out(set, isl_dim_param, 0, isl_set_dim(set, isl_dim_param));

	// isl_set_dump(set);

	std::vector<pesto::maths::Vec2i> depDistanceVector;

	// Project the parameters out instead of dropping the constraints that
	// mention them: dropping is an over-approximation that loses the sign
	// constraints when a dependence distance is bounded by a parameter
	// (d > l here), which made the diagonal family symmetric and produced a
	// spurious negative vector.
	set = isl_set_project_out(set, isl_dim_param, 0, isl_set_dim(set, isl_dim_param));

	// Each ray (m,m), (m,0)... is represented by a basic set; sampling it can
	// return the origin (0,0), losing the direction. Remove the origin so the
	// sample yields the primitive direction (1,1), (1,0) of each ray.
	{
		isl_space *sp = isl_set_get_space(set);
		isl_set *origin = isl_set_universe(isl_space_copy(sp));
		isl_local_space *lsp = isl_local_space_from_space(sp);
		// i0 = 0
		isl_constraint *e0 = isl_constraint_alloc_equality(isl_local_space_copy(lsp));
		e0 = isl_constraint_set_coefficient_si(e0, isl_dim_set, 0, 1);
		origin = isl_set_add_constraint(origin, e0);
		// i1 = 0
		isl_constraint *e1 = isl_constraint_alloc_equality(isl_local_space_copy(lsp));
		e1 = isl_constraint_set_coefficient_si(e1, isl_dim_set, 1, 1);
		origin = isl_set_add_constraint(origin, e1);
		isl_local_space_free(lsp);
		set = isl_set_subtract(set, origin);
	}

	// Sampling one point per basic set can miss illegal directions when a basic
	// set spans several (durbin: {[i0,1] : 0<=i0<=1} holds (1,1) and (0,1), and
	// only the former was sampled). Test the whole set against the cone.
	{
		isl_space *sp = isl_set_get_space(set);
		isl_set *cone = isl_set_universe(isl_space_copy(sp));
		isl_local_space *ls = isl_local_space_from_space(sp);
		isl_constraint *c1 = isl_constraint_alloc_inequality(isl_local_space_copy(ls));
		c1 = isl_constraint_set_coefficient_si(c1, isl_dim_set, 1, 1); // y >= 0
		cone = isl_set_add_constraint(cone, c1);
		isl_constraint *c2 = isl_constraint_alloc_inequality(ls);
		c2 = isl_constraint_set_coefficient_si(c2, isl_dim_set, 0, 1); // x - y >= 0
		c2 = isl_constraint_set_coefficient_si(c2, isl_dim_set, 1, -1);
		cone = isl_set_add_constraint(cone, c2);
		isl_set *outside = isl_set_subtract(isl_set_copy(set), cone);
		int empty = isl_set_is_empty(outside);
		isl_set_free(outside);
		if (!empty) {
			logger->debug("dependence vectors outside the (1,0)-(1,1) cone");
			isl_set_free(set);
			/* … libérations habituelles … */
			return std::nullopt;
		}
	}

	// Helper function for isl_set_foreach_basic_set

	isl_set_foreach_basic_set(
		set,
		[](__isl_take isl_basic_set *bset, void *user) -> isl_stat {
			auto *depDistanceVector = static_cast<std::vector<pesto::maths::Vec2i> *>(user);
			isl_point *point = isl_basic_set_sample_point(bset);

			isl_val *x = isl_point_get_coordinate_val(point, isl_dim_set, 0);
			isl_val *y = isl_point_get_coordinate_val(point, isl_dim_set, 1);

			int vx = isl_val_get_num_si(x);
			int vy = isl_val_get_num_si(y);
			int g = std::gcd(std::abs(vx), std::abs(vy));
			if (g > 1) {
				vx /= g;
				vy /= g;
			}
			depDistanceVector->push_back(pesto::maths::Vec2i(vx, vy));

			isl_val_free(x);
			isl_val_free(y);
			isl_point_free(point);

			return isl_stat_ok;
		},
		&depDistanceVector);

	{
		this->printer = isl_printer_print_set(this->printer, set);
		char *str = isl_printer_get_str(this->printer);
		this->logger->debug("projected deltas: {}", str);
		free(str);
		this->printer = isl_printer_flush(this->printer);
	}

	isl_set_free(set);

	isl_union_map_free(sanitized_dep_flow);
	isl_union_map_free(dep_flow);
	isl_union_set_free(deltas);

	isl_union_map_free(schedules);
	isl_union_map_free(named_schedules);
	isl_union_map_free(writes);
	isl_union_map_free(mayWrites);
	isl_union_map_free(reads);
	isl_union_map_free(empty);
	isl_union_set_free(domains);
	// end of isl computations

	if (depDistanceVector.empty()) {
		return std::nullopt; // no dependences found
	}

	// Now we must check that all the vectors are contained in the cone of the vectors (1, 0) and (1, 1)
	for (auto &vec : depDistanceVector) {
		// condition is vec.y >= 0 and (vec.x - vec.y) >= 0
		if (!(vec.y >= 0 && (vec.x - vec.y) >= 0)) {
			return std::nullopt; // can't perform trapezoidal tiling
		}
	}

	// then we return the vector with the steepest slope
	pesto::maths::Vec2i bestVec(1, 0);
	for (const auto &vec : depDistanceVector) {
		if (vec.x != 0 && vec.y / vec.x > bestVec.y / bestVec.x) {
			bestVec = vec;
		}
	}
	return std::make_optional(bestVec); // return the best vector
}

std::pair<std::string, std::string> TpzTilingPass::computeInnerMinMaxStr(__isl_keep isl_set *innerDomain,
																		 unsigned dimIdx) {
	using pesto::ptile::NameHelper;

	// compute bounds of the inner loop for the current loop
	isl_set *set = isl_set_copy(innerDomain);

	// move all dims from set to param except the one we want to compute the min/max

	set = isl_set_move_dims(set, isl_dim_param, isl_set_dim(set, isl_dim_param), isl_dim_set, 0, dimIdx);

	isl_pw_aff *min = isl_set_dim_min(isl_set_copy(set), 0);
	isl_pw_aff *max = isl_set_dim_max(isl_set_copy(set), 0);

	this->printer = isl_printer_set_output_format(this->printer, ISL_FORMAT_C);

	this->printer = isl_printer_flush(this->printer);
	this->printer = isl_printer_print_pw_aff(this->printer, min);
	char *min_str = isl_printer_get_str(this->printer);

	this->printer = isl_printer_flush(this->printer);
	this->printer = isl_printer_print_pw_aff(this->printer, max);
	char *max_str = isl_printer_get_str(this->printer);

	std::pair<std::string, std::string> minMaxPair = std::make_pair(std::string(min_str), std::string(max_str));
	isl_pw_aff_free(min);
	isl_pw_aff_free(max);
	isl_set_free(set);

	free(min_str);
	free(max_str);

	return minMaxPair;
}

// Scalars written in the region are not indexed by the point: without this,
// every slice shares one location (symm's temp2).
static void collectScalarWrites(osl_scop_p scop, std::vector<std::string> &names) {
	osl_arrays_p arrays = (osl_arrays_p)osl_generic_lookup(scop->extension, OSL_URI_ARRAYS);
	if (!arrays)
		return;
	for (osl_statement_p s = scop->statement; s; s = s->next)
		for (osl_relation_list_p a = s->access; a; a = a->next) {
			if (!a->elt)
				continue;
			if (a->elt->type != OSL_TYPE_WRITE && a->elt->type != OSL_TYPE_MAY_WRITE)
				continue;
			if (a->elt->nb_output_dims > 1)
				continue;
			int id = osl_relation_get_array_id(a->elt);
			for (int k = 0; k < arrays->nb_names; ++k)
				if (arrays->id[k] == id) {
					std::string nm = arrays->names[k];
					if (std::find(names.begin(), names.end(), nm) == names.end())
						names.push_back(nm);
					break;
				}
		}
}

// The skew pass emits a canonical [0,x,0,y,0,z,0] scattering whose even dims are
// constant scalars, so delta dim 0 is identically zero. A raw Pluto schedule
// carries the gap there. Tells the delta projection which layout it faces.
bool TpzTilingPass::isCanonicalLayout(__isl_keep isl_union_set *deltas) {
	struct Probe {
		bool sawNonZero;
		bool sawAny;
	} pr{false, false};
	isl_union_set_foreach_set(
		deltas,
		+[](__isl_take isl_set *d, void *user) -> isl_stat {
			auto *pr = static_cast<Probe *>(user);
			if (isl_set_dim(d, isl_dim_set) < 1) {
				isl_set_free(d);
				return isl_stat_ok;
			}
			pr->sawAny = true;
			isl_set *nonZero = isl_set_copy(d);
			// {dim0 >= 1} U {dim0 <= -1}
			isl_space *sp = isl_set_get_space(d);
			isl_local_space *ls = isl_local_space_from_space(isl_space_copy(sp));
			isl_constraint *cp = isl_constraint_alloc_inequality(isl_local_space_copy(ls));
			cp = isl_constraint_set_coefficient_si(cp, isl_dim_set, 0, 1);
			cp = isl_constraint_set_constant_si(cp, -1);
			isl_set *pos = isl_set_add_constraint(isl_set_copy(nonZero), cp);
			isl_constraint *cn = isl_constraint_alloc_inequality(ls);
			cn = isl_constraint_set_coefficient_si(cn, isl_dim_set, 0, -1);
			cn = isl_constraint_set_constant_si(cn, -1);
			isl_set *neg = isl_set_add_constraint(nonZero, cn);
			isl_space_free(sp);
			if (!isl_set_is_empty(pos) || !isl_set_is_empty(neg))
				pr->sawNonZero = true;
			isl_set_free(pos);
			isl_set_free(neg);
			isl_set_free(d);
			return isl_stat_ok;
		},
		&pr);
	return pr.sawAny && !pr.sawNonZero;
}

// A scalar access in osl carries only the array-identifier dimension: no
// subscripts. Such a location is not indexed by the point, so it cannot survive
// the reduction being tiled — every point of a tile reuses it (symm's temp2).
static bool hasScalarWrite(osl_statement_p stmt) {
	for (osl_relation_list_p a = stmt->access; a; a = a->next) {
		if (!a->elt)
			continue;
		if (a->elt->type != OSL_TYPE_WRITE && a->elt->type != OSL_TYPE_MAY_WRITE)
			continue;
		if (a->elt->nb_output_dims <= 1)
			return true;
	}
	return false;
}

bool TpzTilingPass::isReductionTileable(osl_scop_p scop) {

	for (osl_statement_p s = scop->statement; s; s = s->next) {
		std::string acc;
		for (osl_relation_list_p a = s->access; a; a = a->next) {
			if (!a->elt) {
				acc += " <null>";
				continue;
			}
			const char *k = a->elt->type == OSL_TYPE_WRITE ? "W" : a->elt->type == OSL_TYPE_MAY_WRITE ? "MW" : "R";
			acc += " " + std::string(k) + "(out=" + std::to_string(a->elt->nb_output_dims) + ")";
		}
		logger->debug("stmt depth={}:{}", (int)s->domain->nb_output_dims, acc);
	}

	using pesto::ir::converter::Scop2Isl;
	Scop2Isl irHelper(this->ctx);
	irHelper.convert(scop);

	isl_union_map *schedules = irHelper.getSchedules();
	isl_union_map *writes = irHelper.getWrites();
	isl_union_map *reads = irHelper.getReads();
	isl_union_map *mayWrites = irHelper.getMayWrites();
	isl_union_set *domains = irHelper.getDomains();
	isl_union_map *empty = isl_union_map_empty(irHelper.getParamSpace());

	// named schedules (same as computeDepDistanceVector)
	isl_map_list *schedule_list = isl_union_map_get_map_list(schedules);
	isl_union_map *named_schedules = isl_union_map_empty_ctx(this->ctx);
	for (isl_size i = 0; i < isl_map_list_n_map(schedule_list); ++i) {
		isl_map *map = isl_map_list_get_at(schedule_list, i);
		map = isl_map_set_tuple_name(map, isl_dim_out, isl_map_get_tuple_name(map, isl_dim_in));
		named_schedules = isl_union_map_add_map(named_schedules, map);
	}
	isl_map_list_free(schedule_list);

	// must-flow dependences
	isl_union_access_info *access = isl_union_access_info_from_sink(isl_union_map_copy(reads));
	access = isl_union_access_info_set_must_source(access, isl_union_map_copy(writes));
	access = isl_union_access_info_set_schedule_map(access, isl_union_map_copy(schedules));
	isl_union_flow *flow = isl_union_access_info_compute_flow(access);
	isl_union_map *dep_flow = isl_union_flow_get_must_dependence(flow);
	isl_union_flow_free(flow);

	dep_flow = isl_union_map_apply_range(dep_flow, isl_union_map_copy(named_schedules));
	dep_flow = isl_union_map_apply_domain(dep_flow, isl_union_map_copy(named_schedules));

	// sanitize tuple names
	isl_union_map *sanitized = nullptr;
	isl_map_list *list = isl_union_map_get_map_list(dep_flow);
	for (int i = 0; i < isl_map_list_n_map(list); ++i) {
		isl_map *map = isl_map_list_get_map(list, i);
		map = isl_map_set_tuple_name(map, isl_dim_in, nullptr);
		map = isl_map_set_tuple_name(map, isl_dim_out, nullptr);
		if (!sanitized)
			sanitized = isl_union_map_from_map(isl_map_copy(map));
		else
			sanitized = isl_union_map_add_map(sanitized, isl_map_copy(map));
		isl_map_free(map);
	}
	isl_map_list_free(list);
	isl_union_map_free(dep_flow);

	isl_union_set *deltas = isl_union_map_deltas(sanitized);

	bool canonical = isCanonicalLayout(deltas);
	struct P {
		isl_ctx *ctx;
		isl_set *acc;
		bool canonical;
	} p{this->ctx, nullptr, canonical}; // ← 3e champ
	isl_union_set_foreach_set(
		deltas,
		+[](__isl_take isl_set *d, void *user) -> isl_stat {
			auto *p = static_cast<P *>(user);
			int nd = isl_set_dim(d, isl_dim_set);
			if (p->canonical) {
				// [0,x,0,y,0,z,0] from the skew pass: loop axes at 1, 3, 5.
				if (nd < 6) {
					isl_set_free(d);
					return isl_stat_ok;
				}
				d = isl_set_project_out(d, isl_dim_set, 6, nd - 6);
				d = isl_set_project_out(d, isl_dim_set, 4, 1);
				d = isl_set_project_out(d, isl_dim_set, 2, 1);
				d = isl_set_project_out(d, isl_dim_set, 0, 1);
			} else {
				// Raw Pluto schedule: gap at 0, then the space dims.
				if (nd < 3) {
					isl_set_free(d);
					return isl_stat_ok;
				}
				if (nd > 3)
					d = isl_set_project_out(d, isl_dim_set, 3, nd - 3);
			}
			// now dims are (x, y, z)
			d = isl_set_set_tuple_name(d, nullptr);
			if (!p->acc)
				p->acc = d;
			else
				p->acc = isl_set_union(p->acc, d);
			return isl_stat_ok;
		},
		&p);

	// No exploitable deltas means no analysis, which is not an authorisation.
	bool tileable = false;
	logger->debug("isReductionTileable: p.acc={}", p.acc ? "set" : "null");
	if (p.acc) {
		isl_set *set = isl_set_coalesce(p.acc);
		// Project the parameters out instead of dropping the constraints that
		// mention them: dropping is an over-approximation that loses the sign
		// constraints when a dependence distance is bounded by a parameter
		// (d > l here), which made the diagonal family symmetric and produced a
		// spurious negative vector.
		set = isl_set_project_out(set, isl_dim_param, 0, isl_set_dim(set, isl_dim_param));

		// A dependence carried by z (Δz != 0) that ALSO moves in x or y
		// (Δx != 0 or Δy != 0) means the reduction index indexes another cell;
		// tiling z would reorder those. The pure accumulation (0,0,Δz) is fine.
		// Build {Δz != 0 and (Δx != 0 or Δy != 0)}; if it intersects the deltas,
		// z is NOT tileable.
		auto nz = [&](int dim) -> isl_set * {
			isl_space *sp = isl_set_get_space(set);
			isl_set *pos = isl_set_universe(isl_space_copy(sp));
			isl_constraint *cp = isl_constraint_alloc_inequality(isl_local_space_from_space(isl_space_copy(sp)));
			cp = isl_constraint_set_coefficient_si(cp, isl_dim_set, dim, 1);
			cp = isl_constraint_set_constant_si(cp, -1);
			pos = isl_set_add_constraint(pos, cp);
			isl_set *neg = isl_set_universe(isl_space_copy(sp));
			isl_constraint *cn =
				isl_constraint_alloc_inequality(isl_local_space_from_space(isl_space_copy(sp))); // ← AJOUTE copy
			cn = isl_constraint_set_coefficient_si(cn, isl_dim_set, dim, -1);
			cn = isl_constraint_set_constant_si(cn, -1);
			neg = isl_set_add_constraint(neg, cn);
			isl_space_free(sp);
			return isl_set_union(pos, neg);
		};

		/* auto neg = [&](isl_set *s, int dim) -> isl_set * {
			isl_space *sp = isl_set_get_space(s);
			isl_constraint *c =
				isl_constraint_alloc_inequality(isl_local_space_from_space(isl_space_copy(sp)));
			c = isl_constraint_set_coefficient_si(c, isl_dim_set, dim, -1);
			c = isl_constraint_set_constant_si(c, -1); // v[dim] <= -1
			isl_space_free(sp);
			return isl_set_add_constraint(isl_set_copy(s), c);
		}; */

		isl_set *dzNonZero = nz(2);
		isl_set *dxNonZero = nz(0);
		isl_set *dyNonZero = nz(1);
		isl_set *dxyNonZero = isl_set_union(dxNonZero, dyNonZero);
		isl_set *badTileable = isl_set_intersect(dzNonZero, dxyNonZero);
		badTileable = isl_set_intersect(badTileable, isl_set_copy(set));
		tileable = isl_set_is_empty(badTileable);

		// Statements at maximum depth run inside the reduction loop. If one of them
		// writes a scalar, tiling z scatters it across the tiles: the reset only
		// fires in the tile holding z = 0, and later tiles keep the value left by
		// the previous point.

		isl_set_free(badTileable);
		isl_set_free(set);
	}

	// A reduction shows up as statements of differing depth: the accumulation is
	// one level deeper than the initializations or finalizations around it. A
	// pure stencil has a single depth, and its innermost level is a space
	// dimension that must not be treated as an accumulation.
	int maxDepth = 0, minDepth = INT_MAX;
	for (osl_statement_p s = scop->statement; s; s = s->next) {
		int d = (int)s->domain->nb_output_dims;
		if (d > maxDepth)
			maxDepth = d;
		if (d < minDepth)
			minDepth = d;
	}
	this->hasReduction_ = (maxDepth > minDepth);
	logger->debug("has reduction: {}", this->hasReduction_);

	// Independent of the delta analysis: a statement at maximum depth runs inside
	// the reduction loop, and if it writes a scalar, tiling z scatters it.
	if (tileable) {
		for (osl_statement_p s = scop->statement; s; s = s->next)
			if ((int)s->domain->nb_output_dims == maxDepth && hasScalarWrite(s)) {
				logger->debug("scalar written inside the reduction loop: z not tileable");
				tileable = false;
				this->scalarInReduction_ = true;
				break;
			}
	}

	isl_union_set_free(deltas);
	isl_union_map_free(named_schedules);
	isl_union_map_free(schedules);
	isl_union_map_free(writes);
	isl_union_map_free(reads);
	isl_union_map_free(mayWrites);
	isl_union_map_free(empty);
	isl_union_set_free(domains);

	this->logger->debug("Reduction dimension tileable: {}", tileable);
	return tileable;
}
