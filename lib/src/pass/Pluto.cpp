/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <isl/ctx.h>
#include <isl/flow.h>
#include <isl/id.h>
#include <isl/map.h>
#include <isl/printer.h>
#include <isl/set.h>
#include <isl/space.h>
#include <isl/union_map.h>
#include <isl/union_set.h>

#include <cloog/cloog.h>

#include <osl/scop.h>

#include <pluto/osl_pluto.h>
#include <pluto/pluto.h>
#include <pluto/pluto_internal.h>
#include <pluto/program.h>

#include <pesto/Pluto.hpp>
#include <pesto/bundle/Bundle.hpp>
#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/ir/Scop2Isl.hpp>
#include <pesto/isl_support.h>
#include <pesto/metadata/VarDecl.hpp>
#include <pesto/pass/Pluto.hpp>

#define PLUTO_DEBUG 0
#define PLUTO_QUIET 0

using namespace pesto::pass;

namespace {

struct DimCollector {
	/* per statement: the ORDERED list of its original iterator names */
	std::map<std::string, std::vector<std::string>> names;
};

isl_stat collectDim(__isl_take isl_set *set, void *user) {
	auto *c = static_cast<DimCollector *>(user);
	const char *tuple = isl_set_get_tuple_name(set);
	if (tuple != nullptr) {
		std::vector<std::string> dims;
		unsigned n = isl_set_dim(set, isl_dim_set);
		for (unsigned d = 0; d < n; d++) {
			const char *dn = isl_set_get_dim_name(set, isl_dim_set, d);
			dims.push_back(dn ? dn : "");
		}
		c->names[tuple] = std::move(dims);
	}
	isl_set_free(set);
	return isl_stat_ok;
}

struct ProjBuilder {
	const std::map<std::string, std::vector<std::string>> *origNames;
	isl_union_map *proj;
	bool ok = true;
};

/* pi : S_i[post-pluto dims] -> S_i[original dims], matched BY NAME.
 * Pluto inserts its tile iterators (zT*) at the position of the tiled loop,
 * not at the end — so we cannot project by position. */
isl_stat buildProj(__isl_take isl_set *newSet, void *user) {
	auto *b = static_cast<ProjBuilder *>(user);
	const char *tuple = isl_set_get_tuple_name(newSet);
	if (tuple == nullptr) {
		isl_set_free(newSet);
		return isl_stat_ok;
	}
	auto it = b->origNames->find(tuple);
	if (it == b->origNames->end()) {
		spdlog::warn("legality check: statement '{}' has no pre-pluto counterpart", tuple);
		b->ok = false;
		isl_set_free(newSet);
		return isl_stat_ok;
	}
	const std::vector<std::string> &want = it->second;
	std::set<std::string> wantSet(want.begin(), want.end());

	isl_map *pi = isl_set_identity(isl_set_copy(newSet)); /* S_i[new] -> S_i[new] */

	/* drop every output dim that is not an original iterator; iterate backwards
	 * so the surviving indices stay valid */
	int n_new = isl_map_dim(pi, isl_dim_out);
	for (int d = n_new - 1; d >= 0; --d) {
		const char *dn = isl_map_get_dim_name(pi, isl_dim_out, d);
		if (dn == nullptr || wantSet.count(dn) == 0) {
			pi = isl_map_project_out(pi, isl_dim_out, d, 1);
		}
	}

	if ((size_t)isl_map_dim(pi, isl_dim_out) != want.size()) {
		spdlog::warn("legality check: '{}' kept {} of {} original iterators", tuple, isl_map_dim(pi, isl_dim_out),
					 want.size());
		b->ok = false;
	}

	pi = isl_map_set_tuple_name(pi, isl_dim_out, tuple);
	b->proj = isl_union_map_add_map(b->proj, pi);
	isl_set_free(newSet);
	return isl_stat_ok;
}

} // namespace

PlutoPass::PlutoPass() : IPass("pluto"), pluto() {
	this->pluto = this->pluto.silent(!PLUTO_DEBUG)
					  .quiet(!PLUTO_DEBUG)
					  .debug(PLUTO_DEBUG)
					  .moreDebug(PLUTO_DEBUG)
					  .identity(0)
					  .islDep(1)
					  .tile(0)
					  .diamondTile(0)
					  .parallel(0)
					  .prevector(0)
					  .unrollJam(0);
}

PlutoPass::~PlutoPass() {
	// nothing to do
}

__isl_give isl_union_map *PlutoPass::computeDeps(isl_ctx *ctx, osl_scop_p scop) {
	pesto::ir::converter::Scop2Isl conv(ctx);
	conv.convert(scop);

	isl_union_map *sched = conv.getSchedules();
	isl_union_map *reads = conv.getReads();
	isl_union_map *writes = isl_union_map_union(conv.getWrites(), conv.getMayWrites());

	/* pairs of accesses hitting the same array element, at least one a write */
	isl_union_map *wr =
		isl_union_map_apply_range(isl_union_map_copy(writes), isl_union_map_reverse(isl_union_map_copy(reads)));
	isl_union_map *rw =
		isl_union_map_apply_range(isl_union_map_copy(reads), isl_union_map_reverse(isl_union_map_copy(writes)));
	isl_union_map *ww =
		isl_union_map_apply_range(isl_union_map_copy(writes), isl_union_map_reverse(isl_union_map_copy(writes)));

	isl_union_map *conflict = isl_union_map_union(wr, isl_union_map_union(rw, ww));

	/* keep only the pairs ordered by the ORIGINAL schedule */
	isl_union_map *before = isl_union_map_lex_lt_union_map(isl_union_map_copy(sched), isl_union_map_copy(sched));
	isl_union_map *deps = isl_union_map_intersect(conflict, before);
	deps = isl_union_map_coalesce(deps);

	isl_union_map_free(sched);
	isl_union_map_free(reads);
	isl_union_map_free(writes);
	return deps;
}

__isl_give isl_union_map *PlutoPass::projectedSchedule(isl_ctx *ctx, osl_scop_p scop,
													   __isl_keep isl_union_set *origDomains, bool *reliable) {
	*reliable = true;

	pesto::ir::converter::Scop2Isl conv(ctx);
	conv.convert(scop);

	isl_union_set *newDomains = conv.getDomains();
	isl_union_map *sched = conv.getSchedules();

	DimCollector c;
	isl_union_set_foreach_set(origDomains, &collectDim, &c);

	ProjBuilder b{&c.names, isl_union_map_empty(isl_union_set_get_space(newDomains)), true};
	isl_union_set_foreach_set(newDomains, &buildProj, &b);
	if (!b.ok) {
		*reliable = false;
	}

	isl_union_map *schedProj = isl_union_map_apply_domain(isl_union_map_copy(sched), b.proj);

	isl_union_set *covered = isl_union_map_domain(isl_union_map_copy(schedProj));
	if (isl_union_set_is_equal(covered, origDomains) != isl_bool_true) {
		logger->warn("legality check: projected schedule does not cover all original domains");
		*reliable = false;
	}
	isl_union_set_free(covered);

	isl_union_map_free(sched);
	isl_union_set_free(newDomains);
	return schedProj;
}

__isl_give isl_union_map *PlutoPass::violatedDeps(isl_ctx *ctx, osl_scop_p scop, __isl_keep isl_union_map *deps,
												  __isl_keep isl_union_set *origDomains, bool *reliable) {
	isl_union_map *schedProj = this->projectedSchedule(ctx, scop, origDomains, reliable);

	isl_union_map *before =
		isl_union_map_lex_lt_union_map(isl_union_map_copy(schedProj), isl_union_map_copy(schedProj));
	isl_union_map *violated = isl_union_map_subtract(isl_union_map_copy(deps), before);
	violated = isl_union_map_coalesce(violated);

	isl_union_map_free(schedProj);
	return violated;
}

int PlutoPass::findParallelDim(isl_ctx *ctx, osl_scop_p scop, __isl_keep isl_union_map *deps,
							   __isl_keep isl_union_set *origDomains) {
	bool reliable = false;
	isl_union_map *schedProj = this->projectedSchedule(ctx, scop, origDomains, &reliable);
	if (!reliable) {
		logger->warn("parallelism detection: unreliable schedule projection, giving up");
		isl_union_map_free(schedProj);
		return -1;
	}

	/* Dependences in the SCATTERING space. schedProj maps original iterations to
	 * scattering points, so this is exact for the pairs — but the RANGE must come
	 * from the post-pluto schedule, whose tile dims (zT) are not projected out. */
	isl_union_map *sdeps = isl_union_map_apply_domain(isl_union_map_copy(deps), isl_union_map_copy(schedProj));
	sdeps = isl_union_map_apply_range(sdeps, isl_union_map_copy(schedProj));
	sdeps = isl_union_map_coalesce(sdeps);

	/* range of the PROJECTED schedule: expressed over the original iterators,
	 * where a scalar dim like pluto's "0/42" really is a single point.
	 * fullSched keeps the zT tile iterators, and isl cannot see that the dim
	 * collapses — it reports an interval, and the scalar test fails. */
	isl_union_set *rangeSet = isl_union_map_range(isl_union_map_copy(schedProj));
	isl_set *range = isl_set_from_union_set(rangeSet);
	int ndim = isl_set_dim(range, isl_dim_set);

	isl_map *D = (isl_union_map_n_map(sdeps) > 0) ? isl_map_from_union_map(isl_union_map_copy(sdeps)) : nullptr;

	int found = -1;

	if (D != nullptr) {
		isl_printer *p = isl_printer_to_str(ctx);
		p = isl_printer_set_output_format(p, ISL_FORMAT_ISL);
		p = isl_printer_print_map(p, D);
		char *s = isl_printer_get_str(p);
		logger->warn("scattering deps:\n{}", s);
		free(s);
		isl_printer_free(p);
	}

	for (int d = 0; d < ndim && found < 0; d++) {
		/* (a) real loop? */
		isl_set *proj = isl_set_copy(range);
		if (d + 1 < ndim) {
			proj = isl_set_project_out(proj, isl_dim_set, d + 1, ndim - d - 1);
		}
		if (d > 0) {
			proj = isl_set_project_out(proj, isl_dim_set, 0, d);
		}
		isl_bool single = isl_set_is_singleton(proj);
		isl_set_free(proj);
		if (single != isl_bool_false) {
			logger->warn("  dim {} (t{}): scalar, skipped", d, d + 1);
			continue;
		}

		/* (b) carried at d? */
		bool parallel = true;
		if (D != nullptr) {
			isl_map *carried = isl_map_copy(D);
			for (int e = 0; e < d; e++) {
				carried = isl_map_equate(carried, isl_dim_in, e, isl_dim_out, e);
			}
			carried = isl_map_order_lt(carried, isl_dim_in, d, isl_dim_out, d);

			if (d == 3) {
				isl_printer *p = isl_printer_to_str(ctx);
				p = isl_printer_set_output_format(p, ISL_FORMAT_ISL);
				p = isl_printer_print_map(p, carried);
				char *s = isl_printer_get_str(p);
				logger->warn("carried at d=3:\n{}", s);
				free(s);
				isl_printer_free(p);
			}

			parallel = (isl_map_is_empty(carried) == isl_bool_true);
			isl_map_free(carried);
		}

		logger->warn("  dim {} (t{}): loop, {}", d, d + 1, parallel ? "PARALLEL" : "carries dependences");
		if (parallel) {
			found = d;
		}
	}

	if (D != nullptr) {
		isl_map_free(D);
	}
	isl_set_free(range);
	// isl_union_map_free(fullSched);
	isl_union_map_free(sdeps);
	isl_union_map_free(schedProj);
	return found;
}

std::set<std::string> PlutoPass::inputIR() const {
	return {pesto::ir::OpenScopIR::getID()};
}
std::string PlutoPass::outputIR() const {
	return pesto::ir::OpenScopIR::getID();
}

pesto::Bundle PlutoPass::apply(pesto::Bundle &&bundle) {
	ir::OpenScopIR *scopIR = dynamic_cast<ir::OpenScopIR *>(bundle.get_ir());
	osl_scop_p c_scop = scopIR->c_scop();

	if (osl_generic_lookup(c_scop->extension, OSL_URI_LOOP) != nullptr) {
		osl_generic_remove(&c_scop->extension, OSL_URI_LOOP);
	}
	if (osl_generic_lookup(c_scop->extension, OSL_URI_PLUTO_UNROLL) != nullptr) {
		osl_generic_remove(&c_scop->extension, OSL_URI_PLUTO_UNROLL);
	}

	/* ---- snapshot BEFORE pluto: the original schedule is still in the scop ---- */
	isl_ctx *ctx = isl_ctx_alloc();
	isl_union_map *deps = nullptr;
	isl_union_set *origDomains = nullptr;
	if (this->checkLegality) {
		{
			pesto::ir::converter::Scop2Isl conv(ctx);
			conv.convert(c_scop);
			origDomains = conv.getDomains();
		}
		deps = this->computeDeps(ctx, c_scop);
	}

	this->pluto.run(c_scop);

	osl_loop_p ll = (osl_loop_p)osl_generic_lookup(c_scop->extension, OSL_URI_LOOP);
	if (ll == nullptr) {
		logger->warn("pluto: no loop extension — no parallel loop was recorded");
	} else {
		for (osl_loop_p l = ll; l != NULL; l = l->next) {
			logger->info("pluto: loop iter='{}' directive={} nb_stmts={}", l->iter, l->directive, l->nb_stmts);
		}
	}

	/* ---- verify AFTER pluto ---- */
	if (deps != nullptr) {
		bool reliable = false;
		isl_union_map *violated = this->violatedDeps(ctx, c_scop, deps, origDomains, &reliable);
		bool illegal = (isl_union_map_is_empty(violated) == isl_bool_false);

		std::string report;
		if (illegal) {
			isl_printer *p = isl_printer_to_str(ctx);
			p = isl_printer_set_output_format(p, ISL_FORMAT_ISL);
			p = isl_printer_print_union_map(p, violated);
			char *str = isl_printer_get_str(p);
			report = str ? str : "<unprintable>";
			free(str);
			isl_printer_free(p);
		}

		int pd = this->findParallelDim(ctx, c_scop, deps, origDomains);
		osl_loop_p ll = (osl_loop_p)osl_generic_lookup(c_scop->extension, OSL_URI_LOOP);

		if (pd >= 0) {
			std::string iter = "t" + std::to_string(pd + 1);
			if (ll != nullptr) {
				if (iter != std::string(ll->iter)) {
					// A scop can hold several bands with different parallel
					// dimensions; this analysis is global and cannot arbitrate
					// per entry. Leave the names alone — clast_annotate_loops
					// matches by name and falls back on the statement ids.
					logger->debug("pluto named '{}' parallel, global analysis says '{}'; keeping pluto's", ll->iter,
								  iter);
				}
			} else {
				logger->info("pluto found no parallel loop; '{}' is parallel — build an osl_loop entry", iter);
				osl_loop_p nl = osl_loop_malloc();
				nl->iter = strdup(iter.c_str());
				// Every statement deep enough to carry that dimension.
				int n = 0, num = 0;
				for (osl_statement_p s = c_scop->statement; s; s = s->next)
					if ((int)s->scattering->nb_output_dims > pd)
						n++;
				nl->nb_stmts = n;
				nl->stmt_ids = (int *)malloc((n ? n : 1) * sizeof(int));
				int idx = 0;
				for (osl_statement_p s = c_scop->statement; s; s = s->next) {
					num++;
					if ((int)s->scattering->nb_output_dims > pd)
						nl->stmt_ids[idx++] = num;
				}
				nl->directive = CLAST_PARALLEL_OMP;
				nl->private_vars = strdup("");
				osl_generic_add(&c_scop->extension, osl_generic_shell(nl, osl_loop_interface()));
			}
		} else if (ll != nullptr) {
			logger->warn("pluto named '{}' parallel, but no dim is; dropping the extension", ll->iter);
			osl_generic_remove(&c_scop->extension, OSL_URI_LOOP);
		}

		/* free EVERYTHING before any throw: the ctx must outlive no isl object */
		isl_union_map_free(violated);
		isl_union_map_free(deps);
		isl_union_set_free(origDomains);

		if (!reliable) {
			logger->error("legality check could not run (schedule/domain mismatch). "
						  "This is a bug in the checker, NOT a Pluto bug — the schedule was not verified.");
			throw std::runtime_error("pesto: legality check failed to run");
		}
		if (illegal) {
			logger->error("Pluto produced an ILLEGAL schedule. Violated dependences:\n{}", report);
			throw std::runtime_error("pluto: illegal schedule");
		}
		logger->info("Schedule legality check passed");
	}
	isl_ctx_free(ctx);

	/* ---- unchanged: scatnames -> varDecl ---- */
	auto &varDecl = bundle.metadata.get<pesto::bundle::metadata::VarDecl>();
	osl_scatnames_p scatnames = (osl_scatnames_p)osl_generic_lookup(c_scop->extension, OSL_URI_SCATNAMES);
	if (scatnames != nullptr) {
		for (size_t i = 0; i < osl_strings_size(scatnames->names); i++) {
			varDecl.addVar(scatnames->names->string[i]);
		}
	}
	varDecl.addVar("lbp, ubp");
	varDecl.addVar("lbv, ubv");
	varDecl.addVar("lb, ub");

	return std::move(bundle);
}

pesto::PlutoWrapper &PlutoPass::getPluto() {
	return this->pluto;
}
