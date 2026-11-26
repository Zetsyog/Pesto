/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <cloog/isl/cloog.h>

#include <osl/osl.h>

#include <isl/aff.h>
#include <isl/constraint.h>
#include <isl/ctx.h>
#include <isl/local_space.h>
#include <isl/options.h>
#include <isl/val.h>

#include <trahrhe/trahrhe.h>

#include <pesto/bundle/byproduct/HeaderFile.hpp>
#include <pesto/clast_support.h>
#include <pesto/ir/ClastIR.hpp>
#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/isl_support.h>
#include <pesto/metadata/FileInfo.hpp>
#include <pesto/metadata/Macro.hpp>
#include <pesto/metadata/VarDecl.hpp>
#include <pesto/osl_support.h>
#include <pesto/pass/CloogCodegen.hpp>
#include <pesto/pass/Pass.hpp>
#include <pesto/pass/Pluto.hpp>
#include <pesto/pass/clast/ParamTiling.hpp>

using pesto::Bundle;
using pesto::pass::ClastParamTilingPass;

namespace {

/* Trahrhe emits the polynomials of EVERY dimension of the domain it is given,
 * including dimensions BORROWED from another group — which another header
 * already defines. Guard each function by name: the first header to define it
 * wins, later ones become no-ops. */
std::string guardTrahrheHeader(const std::string &src) {
	static const std::string kSig = "static inline long int ";

	size_t first = src.find(kSig);
	if (first == std::string::npos) {
		return src;
	}

	std::string out = src.substr(0, first); /* prologue: #include, macros */

	for (size_t pos = first; pos != std::string::npos;) {
		size_t next = src.find(kSig, pos + kSig.size());
		size_t end = (next == std::string::npos) ? src.size() : next;

		size_t nameStart = pos + kSig.size();
		size_t paren = src.find('(', nameStart);
		std::string name = (paren == std::string::npos) ? "" : src.substr(nameStart, paren - nameStart);

		if (name.empty()) {
			out.append(src, pos, end - pos);
		} else {
			out += "#ifndef PESTO_TRAHRHE_FN_" + name + "\n";
			out += "#define PESTO_TRAHRHE_FN_" + name + "\n";
			out.append(src, pos, end - pos);
			out += "#endif /* PESTO_TRAHRHE_FN_" + name + " */\n\n";
		}

		pos = next;
	}
	return out;
}

} // namespace

pesto::pass::ClastParamTilingPass::ClastParamTilingPass()
	: IPass("param-tiling")
	, gen_trahrhe(false)
	, trahrhe_use_dichotomy(true)
	, tilingTypes()
	, boundComputeMethod(ptile::codegen::BoundComputeMethod::FILL_LAST)
	, boundCorrectionStrategy(ptile::codegen::BoundCorrectionStrategy::NO_CORRECTION)
	, volumeCheckStrategy(ptile::codegen::VolumeCheckStrategy::DYNAMIC_TILE_UNTIL_THRESHOLD)
	, volumeCheckTrigger(VolumeCheckTrigger::FIRST_OUTER_ITERATOR_DEPENDENCY)
	, volumeCheckThreshold(std::nullopt) {
	this->gen_clast_pass = std::make_unique<GenClastPass>();
	this->gen_clast_pass->cloogF = -1;
	this->gen_clast_pass->cloogL = 1;
	// Keep guards for empty parameter domains; generated counting functions
	// are only defined where the iteration domain is nonempty.

	this->ctx = isl_ctx_alloc();
	this->printer = isl_printer_to_str(this->ctx);
}

pesto::pass::ClastParamTilingPass::~ClastParamTilingPass() {
	isl_printer_free(this->printer);
	isl_ctx_free(this->ctx);
}

std::set<std::string> pesto::pass::ClastParamTilingPass::inputIR() const {
	return {pesto::ir::OpenScopIR::getID()};
}

std::string pesto::pass::ClastParamTilingPass::outputIR() const {
	return pesto::ir::ClastIR::getID();
}

Bundle pesto::pass::ClastParamTilingPass::apply(Bundle &&bundle) {
	auto *scopIR = dynamic_cast<pesto::ir::OpenScopIR *>(bundle.get_ir());
	osl_scop_p scop = scopIR->c_scop();

	std::filesystem::path outputFile("anonymous.c");
	if (bundle.metadata.has<pesto::bundle::metadata::FileInfo>()) {
		auto &fileInfo = bundle.metadata.get<pesto::bundle::metadata::FileInfo>();
		outputFile = fileInfo.output;
	}

	if (this->areTileSkewed(scop)) {
		// TODO: add a flag to preven pesto to do this
		logger->info("Detected skewing over tiling loops. Switching to dynamic scheduling. ");

		this->clearTilingTypes();
		this->pushTilingType(ParamTilingType::SCHEDULED);
		this->pushTilingType(ParamTilingType::SCHEDULED);
		this->pushTilingType(ParamTilingType::RECTANGULAR);

		// Undo tile skewing ?
		this->undoTileSkewing(scop);
	}

	// generate the clast
	logger->info("Generating clast");

	bundle = this->gen_clast_pass->apply(std::move(bundle));
	if (bundle.empty()) {
		logger->error("Clast generation failed");
		throw std::runtime_error("Clast generation failed");
	}
	if (bundle.get_ir()->id() != pesto::ir::ClastIR::getID()) {
		logger->error("Expected ClastIR after GenClastPass, got {}", bundle.get_ir()->id().c_str());
		throw std::runtime_error("Unexpected IR after GenClastPass");
	}

	auto *clastIR = dynamic_cast<pesto::ir::ClastIR *>(bundle.get_ir());

	CloogProgram *cloogProgram = clastIR->getCloogProgram();
	CloogOptions *cloogOptions = clastIR->getCloogOptions();
	struct clast_root *root = (struct clast_root *)clastIR->get();

	this->names.clear();
	this->statements.clear();
	this->varList.clear();
	this->insertedStmts = 0;
	this->trahrheDomainList_.clear();

	if (root == nullptr) {
		logger->error("Clast root is null");
		throw std::runtime_error("Clast root is null");
	}

	// Then we must create group of statement that can be tiled together
	// i.e tiling bands
	this->logger->info("Computing stmt groups...");
	auto stmtGroups = this->computeStmtGroups(cloogProgram, root);

	this->logger->info("Found {} statements groups", stmtGroups.size());

	if (stmtGroups.size() == 0) {
		logger->warn("No statement groups found, skipping tiling");
		return std::move(bundle);
	}

	for (auto &group : stmtGroups) {
		// First we need to associate for each tiled dimension its inner and outer
		// for
		this->associateTilingFor(root, *group);

		// compute the domain of each group without the tiling dimensions, this will be used for trahrhe
		this->computeUntiledDomain(cloogProgram, *group);

		// compute tiling data bound expressions
		for (auto &data : group->tilingData) {
			this->computeTilingDataBoundExpr(cloogProgram, *group, *data);
		}
	}

	this->logger->info("Applying tiling...");

	// then for each statement group we apply the tiling
	for (auto &group : stmtGroups) {
		this->tileGroup(cloogProgram, cloogOptions, root, *group);
	}

	for (auto &name : this->names) {
		clastIR->names.push_back(name);
	}

	auto &macroData = bundle.metadata.get<pesto::bundle::metadata::Macro>();
	for (auto &pair : this->statements) {
		macroData.defineFunc("S" + std::to_string(pair.first), "", pair.second);
	}

	auto &varDeclData = bundle.metadata.get<pesto::bundle::metadata::VarDecl>();
	for (auto &var : this->varList) {
		varDeclData.addVar(var);
	}

	// compute the maximum trahrhe tile level
	int maxTrahrheTileLevel = 0;
	for (auto &pair : this->trahrheDomainList_) {
		if (pair.first > maxTrahrheTileLevel) {
			maxTrahrheTileLevel = pair.first;
		}
	}
	for (int i = 0; i <= maxTrahrheTileLevel; i++) {
		macroData.nonProcessedList.push_back("#ifndef DIV" + std::to_string(i) +
											 "\n"
											 "#define DIV" +
											 std::to_string(i) +
											 " 8\n"
											 "#endif");
	}

	if (this->gen_trahrhe) {
		int domainCount = 0;

		std::filesystem::path outputDir = ".";
		std::string filename = "header.";
		if (!outputFile.stem().string().empty()) {
			filename = outputFile.stem().string() + ".";
		}
		if (outputFile.has_parent_path()) {
			outputDir = outputFile.parent_path();
		}

		for (auto &pair : this->trahrheDomainList_) {
			std::string header_filename = filename + "trahrhe." + std::to_string(domainCount) + ".h";

			trahrhe_context_t *trahrhe_ctx = trahrhe_context_alloc();
			trahrhe_context_use_dichotomy(trahrhe_ctx, this->trahrhe_use_dichotomy);
			trahrhe_context_set_verbosity(trahrhe_ctx, TRAHRHE_VERBOSITY_INFO);
			trahrhe_context_parse_input_domain(trahrhe_ctx, (char *)pair.second.c_str());
			trahrhe_context_set_tile_level(trahrhe_ctx, pair.first);
			trahrhe_context_set_gen_header_level(trahrhe_ctx, -1);
			size_t trahrhe_len = 0;
			char **trahrhe_expr = trahrhe_get_inverted_polynomials_expr(trahrhe_ctx, &trahrhe_len);
			if (trahrhe_len == 0) {
				logger->error("Could not compute trahrhe expr");
				continue;
			}
			std::string header_path = (outputDir / header_filename).string();
			logger->debug("trahrhe header = {}", header_path.c_str());
			logger->debug("trahrhe level  = {}", pair.first);
			logger->debug("trahrhe domain = {}", pair.second.c_str());

			/* capture trahrhe's output in memory so we can guard it before writing */
			char *trahrhe_buf = nullptr;
			size_t trahrhe_buflen = 0;
			FILE *fp = open_memstream(&trahrhe_buf, &trahrhe_buflen);
			if (fp == NULL) {
				logger->error("Error: Could not open memory stream for {}", header_filename.c_str());
				if (trahrhe_expr != NULL) {
					for (size_t i = 0; i < trahrhe_len; i++) {
						if (trahrhe_expr[i] != NULL) {
							free(trahrhe_expr[i]);
						}
					}
					free(trahrhe_expr);
				}
				continue;
			}
			trahrhe_generate_header(trahrhe_ctx, fp, trahrhe_len, trahrhe_expr);
			fclose(fp); /* flushes into trahrhe_buf */

			std::ofstream ofs(header_path);
			ofs << guardTrahrheHeader(std::string(trahrhe_buf, trahrhe_buflen));
			ofs.close();
			free(trahrhe_buf);

			logger->info("Generated trahrhe header file: {}", header_filename.c_str());

			bundle.add_byproduct(std::make_unique<pesto::bundle::byproduct::HeaderFile>(header_path));

			// TODO: function to generate the header file name
			domainCount++;

			if (trahrhe_expr != NULL) {
				for (size_t i = 0; i < trahrhe_len; i++) {
					if (trahrhe_expr[i] != NULL) {
						free(trahrhe_expr[i]);
					}
				}
				free(trahrhe_expr);
			}
			trahrhe_context_free(trahrhe_ctx);
		}
	}
	// Tile scheduling
	if (this->getTilingType(0) == ParamTilingType::SCHEDULED && this->getTilingType(1) == ParamTilingType::SCHEDULED) {
		// TODO: this should not be hardcoded
		auto scheduleHeaderFile = "schedule.h";

		std::filesystem::path outputDir = ".";
		std::string filename = "anonymous";
		if (!outputFile.stem().string().empty()) {
			filename = outputFile.stem().string();
		}
		if (outputFile.has_parent_path()) {
			outputDir = outputFile.parent_path();
		}
		std::string scheduleFilename = filename + ".trahrhe.schedule.n" + std::to_string(0) + ".h";
		std::filesystem::path scheduleFile = outputDir / scheduleFilename;

		std::filesystem::copy(scheduleHeaderFile, scheduleFile, std::filesystem::copy_options::overwrite_existing);
		std::filesystem::remove(scheduleHeaderFile);

		bundle.add_byproduct(std::make_unique<bundle::byproduct::HeaderFile>(scheduleFile.string()));

		logger->info("Generated schedule file: {}", scheduleFilename.c_str());
	}
	return std::move(bundle);
}

void pesto::pass::ClastParamTilingPass::setBoundComputeMethod(ptile::codegen::BoundComputeMethod method) {
	this->boundComputeMethod = method;
}
void pesto::pass::ClastParamTilingPass::setBoundCorrectionStrategy(ptile::codegen::BoundCorrectionStrategy strategy) {
	this->boundCorrectionStrategy = strategy;
}
void pesto::pass::ClastParamTilingPass::setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy strategy) {
	this->volumeCheckStrategy = strategy;
}

void pesto::pass::ClastParamTilingPass::setVolumeCheckTrigger(VolumeCheckTrigger trigger) {
	this->volumeCheckTrigger = trigger;
}

void pesto::pass::ClastParamTilingPass::setVolumeCheckThreshold(std::string threshold) {
	this->volumeCheckThreshold = threshold;
}

bool pesto::pass::ClastParamTilingPass::areTileSkewed(osl_scop_p &scop) {
	for (osl_statement_p stmt = scop->statement; stmt != nullptr; stmt = stmt->next) {
		if (this->getSkewedTilingDimensions(stmt).has_value()) {
			return true;
		}
	}
	return false;
}

std::optional<std::pair<int, int>> pesto::pass::ClastParamTilingPass::getSkewedTilingDimensions(osl_statement_p stmt) {

	osl_relation_p domain = stmt->domain;
	osl_relation_p scat = stmt->scattering;

	std::vector<int> tilingDims;
	int ndim = domain->nb_output_dims;

	for (int i = 0; i < ndim; i++) {
		// ignore equalities
		if (osl_relation_is_row_equality(domain, i)) {
			continue;
		}
		if (osl_relation_is_tiling_dim(domain, i)) {
			tilingDims.push_back(i);
		}
	}

	int d1 = -1;
	for (int i = 0; i < scat->nb_rows; i++) {
		int involvedTiledDims = 0;
		for (auto &dim : tilingDims) {
			int dimIdx = 1 + scat->nb_output_dims + dim;
			if (!osl_int_zero(scat->precision, scat->m[i][dimIdx])) {
				involvedTiledDims++;
			}
		}
		if (involvedTiledDims == 2) {
			d1 = i;
			break;
		}
	}

	if (d1 == -1) {
		return std::nullopt;
	}

	for (int i = 0; i < scat->nb_rows; i++) {
		if (i == d1) {
			continue;
		}
		for (auto &dim : tilingDims) {
			if (osl_int_eq(scat->precision, scat->m[d1][1 + scat->nb_input_dims + dim],
						   scat->m[i][1 + scat->nb_output_dims + dim])) {
				return std::make_pair(d1, i);
			}
		}
	}

	return std::nullopt;
}

void pesto::pass::ClastParamTilingPass::undoTileSkewing(osl_scop_p &scop) {

	for (osl_statement_p stmt = scop->statement; stmt != nullptr; stmt = stmt->next) {
		auto skewedDims = this->getSkewedTilingDimensions(stmt);
		if (!skewedDims.has_value()) {
			continue;
		}

		logger->info("Undoing tile skewing of dim {} and {}", skewedDims->first, skewedDims->second);
		osl_relation_p scat = stmt->scattering;

		osl_int_t one;
		osl_int_init_set_long(scat->precision, &one, 1);
		osl_int_t tmp;
		osl_int_init(scat->precision, &tmp);

		std::vector<int> firstNonZeroOutputDims;
		std::vector<int> secondNonZeroOutputDims;
		for (int i = 0; i < scat->nb_output_dims; i++) {
			if (osl_int_eq(scat->precision, scat->m[skewedDims->first][1 + scat->nb_input_dims + i], one)) {
				firstNonZeroOutputDims.push_back(i);
			}
			if (osl_int_eq(scat->precision, scat->m[skewedDims->second][1 + scat->nb_input_dims + i], one)) {
				secondNonZeroOutputDims.push_back(i);
			}
		}

		if (firstNonZeroOutputDims.size() == 2 && secondNonZeroOutputDims.size() == 1) {
			if (firstNonZeroOutputDims[1] == secondNonZeroOutputDims[0]) {
				osl_int_set_si(scat->precision,
							   &scat->m[skewedDims->first][1 + scat->nb_input_dims + firstNonZeroOutputDims[1]], 0);
			}
		}

		osl_int_clear(scat->precision, &tmp);
		osl_int_clear(scat->precision, &one);
	}
}

std::vector<std::unique_ptr<pesto::StmtGroup>> ClastParamTilingPass::computeStmtGroups(CloogProgram *cloogProgram,
																					   struct clast_root *root) {
	int stmt_count = 0;
	// this is a list of pointer of user statements in the clast
	// they will be freed with the clast root so no need for memory management
	// here
	struct clast_user_stmt **stmt = clast_get_user_stmt_list(&root->stmt, &stmt_count);
	std::vector<std::unique_ptr<StmtGroup>> stmtGroups;

	this->logger->info("Found {} statements in clast", stmt_count);
	if (stmt_count == 0) {
		return stmtGroups;
	}

	// vector of all our statements
	// we will remove them from this vector as we process them
	std::vector<struct clast_user_stmt *> stmt_to_process;
	for (int i = 0; i < stmt_count; i++) {
		stmt_to_process.push_back(stmt[i]);
	}

	// remove all statements that are not tileable from the list of statement to process
	for (std::vector<struct clast_user_stmt *>::iterator it = stmt_to_process.begin(); it != stmt_to_process.end();) {
		struct clast_for *outermost_extern_for = clast_get_outermost_tiling_for(&(root->stmt), &((*it)->stmt));
		if (outermost_extern_for == NULL) {
			logger->debug("stmt {} is not tileable, no for loop iterating over tile space found",
						  (*it)->statement->number);
			it = stmt_to_process.erase(it);
		} else {
			++it;
		}
	}

	// sort by clast depth
	// deeper last
	std::sort(stmt_to_process.begin(), stmt_to_process.end(),
			  [root](struct clast_user_stmt *a, struct clast_user_stmt *b) {
				  struct clast_for *a_parent_for = clast_get_parent_clast_for(&(root->stmt), &(a->stmt));
				  struct clast_for *b_parent_for = clast_get_parent_clast_for(&(root->stmt), &(b->stmt));

				  return clast_for_depth(&(root->stmt), a_parent_for) < clast_for_depth(&(root->stmt), b_parent_for);
			  });

	this->logger->info("{} statements are tiled", stmt_to_process.size());

	/*
		The algorithm is as follows:
		1. Take the first statement and create a group with it
		2. Mark the statement as processed
		3. For each statement that has not been processed yet
			1. If the statement has the same parent clast_for as the group
				1. Add the statement to the group
				2. Mark the statement as processed

	*/

	// while we have statements to process
	while (stmt_to_process.size() != 0) {
		// list of statements that have the same parent for
		// we pick a statement
		struct clast_user_stmt *stmt = stmt_to_process.back();
		// we mark it as processed
		stmt_to_process.pop_back();

		// get the parent for of the stmt
		struct clast_for *parent_for = clast_get_parent_clast_for((struct clast_stmt *)root, (struct clast_stmt *)stmt);

		// Check is this statement is inside a for loop
		if (parent_for == NULL) {
			// if not we skip it
			continue;
		}

		logger->debug("parent for of stmt {} is {}", stmt->statement->number,
					  parent_for != NULL ? parent_for->iterator : "NULL");

		struct clast_for *outermost_extern_for = clast_get_outermost_tiling_for(&(root->stmt), &(stmt->stmt));
		if (outermost_extern_for == NULL) {
			logger->debug("No for loop iterating over tile space found for stmt {}", stmt->statement->number);
			continue;
		}

		// the statement is inside a for loop
		// we create a group that will contain this statement and all the other statements inside the same for
		auto group = std::make_unique<StmtGroup>();
		group->stmts.push_back(stmt);
		group->stmt_parent_for = parent_for;
		group->id = stmtGroups.size();
		group->first_inner_for = NULL;
		group->outermost_for = outermost_extern_for;

		isl_set *parent_domain = isl_set_from_cloog_domain(parent_for->domain);

		for (struct clast_for *current_for = parent_for; current_for != NULL;
			 current_for = clast_get_parent_clast_for((struct clast_stmt *)root, (struct clast_stmt *)current_for)) {

			isl_size current_ndim = isl_set_dim(isl_set_from_cloog_domain(current_for->domain), isl_dim_set);
			if (isl_set_is_tiling_dim(parent_domain, current_ndim - 1) == isl_bool_true) {
				group->first_inner_for = current_for;
				break;
			}
		}

		logger->debug("first tiling for of group {} is {}", group->id,
					  group->first_inner_for != NULL ? group->first_inner_for->iterator : "NULL");

		// for each statement that has not been processed yet
		for (std::vector<struct clast_user_stmt *>::iterator it = stmt_to_process.begin();
			 it != stmt_to_process.end();) {
			bool add = false;
			for (struct clast_for *current_for =
					 clast_get_parent_clast_for((struct clast_stmt *)root, (struct clast_stmt *)*it);
				 current_for != nullptr; current_for = clast_get_parent_clast_for((struct clast_stmt *)root,
																				  (struct clast_stmt *)current_for)) {
				struct clast_for *stmt_outermost_extern_for =
					clast_get_outermost_tiling_for(&(root->stmt), &(current_for->stmt));
				logger->debug("Comparing parent for of stmt {} with group {} parent for: {} vs {}",
							  (*it)->statement->number, group->id,
							  stmt_outermost_extern_for != NULL ? stmt_outermost_extern_for->iterator : "NULL",
							  group->outermost_for != NULL ? group->outermost_for->iterator : "NULL");
				if (stmt_outermost_extern_for == group->outermost_for) {
					logger->debug("stmt {} has the same outermost tiling for as group {}, adding it to the group",
								  (*it)->statement->number, group->id);
				}
				if (current_for == group->first_inner_for) {
					add = true;
					break;
				}
			}
			if (add) {
				group->stmts.push_back(*it);
				it = stmt_to_process.erase(it);
			} else {
				++it;
			}
		}

		isl_set *group_domain = isl_set_copy(isl_set_from_cloog_domain(group->stmt_parent_for->domain));
		// define param names
		for (int i = 0; i < cloogProgram->names->nb_parameters; i += 1) {
			group_domain = isl_set_set_dim_name(group_domain, isl_dim_param, i, cloogProgram->names->parameters[i]);
		}

		for (isl_size i = 0; i < isl_set_n_dim(group_domain); i += 1) {
			group_domain = isl_set_set_dim_name(group_domain, isl_dim_set, i, cloogProgram->names->scattering[i]);
		}
		group_domain = isl_set_coalesce(group_domain);

		if (logger->level() <= spdlog::level::debug) {
			this->printer = isl_printer_set_output_format(this->printer, ISL_FORMAT_ISL);
			this->printer = isl_printer_print_set(this->printer, group_domain);
			char *str = isl_printer_get_str(this->printer);
			this->printer = isl_printer_flush(this->printer);
			this->logger->debug("Computed group domain: {}", str);
			free(str);
			if (logger->level() <= spdlog::level::trace) {
				this->printer = isl_printer_set_output_format(this->printer, ISL_FORMAT_EXT_POLYLIB);
				this->printer = isl_printer_print_set(this->printer, group_domain);
				str = isl_printer_get_str(this->printer);
				this->printer = isl_printer_flush(this->printer);
				this->logger->debug("Computed group domain in polylib format: {}", str);
				free(str);
			}
		}
		isl_bool empty = isl_set_is_empty(group_domain);
		int tiling_dim_count = isl_set_count_tiling_dimensions(group_domain);
		isl_set_free(group_domain);

		if (empty == isl_bool_true || tiling_dim_count == 0) {
			continue;
		}

		// append group to the list of groups
		stmtGroups.push_back(std::move(group));
	}
	free(stmt);

	this->insertedStmts = stmt_count;

	for (auto &group : stmtGroups) {
		this->logger->debug("group {}", group->id);
		this->logger->debug("parent for: {}", group->stmt_parent_for->iterator);
		this->logger->debug("first tiling for: {}",
							group->first_inner_for != NULL ? group->first_inner_for->iterator : "NULL");
		this->logger->debug("outermost tiling for: {}",
							group->outermost_for != NULL ? group->outermost_for->iterator : "NULL");
		this->logger->debug("stmts: ");
		for (auto stmt : group->stmts) {
			this->logger->debug("\t{}", stmt->statement->number);
		}
	}

	return stmtGroups;
}

void ClastParamTilingPass::tileGroup(CloogProgram *cloogProgram, CloogOptions *cloogOptions, struct clast_root *root,
									 StmtGroup &group) {
	isl_set *group_domain = isl_set_copy(isl_set_from_cloog_domain(group.stmt_parent_for->domain));

	logger->debug("Tiling group {} ", group.id);

	int tiling_dim_count = isl_set_count_tiling_dimensions(group_domain);

	// if there are no tiled dimensions we can skip this group
	if (tiling_dim_count == 0) {
		this->logger->debug("No tiled dimensions found in group");
		return;
	}

	if (group.tilingData.empty()) {
		this->logger->debug("group {} shares all its tiling dimensions with another group, skipping", group.id);
		isl_set_free(group_domain);
		return;
	}

	// Then for each outer for (from outer most to inner most)
	for (auto &data : group.tilingData) {
		logger->debug("Tiling for {} with outer iterator {} at depth {} ", data->id, data->outer_for->iterator,
					  data->tiling_depth);

		this->insertStatements(cloogOptions, root, *data);
		this->replaceOuterForBounds(group, *data);

		this->replaceInnerForBounds(cloogProgram, group, *data);
	}
	this->computeTrahrheDomain(group);

	logger->debug("Generating stmts for group {}", group.id);
	this->generateStatements(group);
	this->declareGroupVariables(group);

	// then we need to handle the parallel loop and update openmp pragma
	this->updateOmpPragma(group);

	isl_set_free(group_domain);
}

void ClastParamTilingPass::associateTilingFor(struct clast_root *root, StmtGroup &group) {
	for (auto *stmt : group.stmts) {
		for (struct clast_for *current_for = clast_get_parent_clast_for(&(root->stmt), &(stmt->stmt));
			 current_for != NULL; current_for = clast_get_parent_clast_for(&(root->stmt), &(current_for->stmt))) {

			struct clast_for *outer_for = clast_find_outer_tiling_for(&(root->stmt), current_for);
			if (outer_for == nullptr) {
				continue; /* not a point loop of a tiled dimension */
			}

			auto shared = this->sharedTilingData_.find(outer_for);
			if (shared != this->sharedTilingData_.end()) {
				group.innerForOf.emplace(shared->second, current_for);
				auto &fors = shared->second->inner_fors;
				if (std::find(fors.begin(), fors.end(), current_for) == fors.end()) {
					fors.push_back(current_for);
				}

				bool owned =
					std::any_of(group.tilingData.begin(), group.tilingData.end(),
								[&](const std::unique_ptr<TilingData> &d) { return d.get() == shared->second; });
				if (!owned && std::find(group.borrowedTilingData.begin(), group.borrowedTilingData.end(),
										shared->second) == group.borrowedTilingData.end()) {
					logger->debug("group {} borrows tiling data '{}' from another group", group.id,
								  shared->second->inner_for->iterator);
					group.borrowedTilingData.push_back(shared->second);
				}
				continue;
			}

			auto data = std::make_unique<TilingData>();
			data->id = group.tilingData.size();
			data->outer_for = outer_for;
			data->inner_for = current_for;
			data->inner_fors.push_back(current_for);

			this->sharedTilingData_[outer_for] = data.get();
			group.innerForOf.emplace(data.get(), current_for);
			group.tilingData.push_back(std::move(data));
		}
	}

	// compute tiling depth for each tiling data
	for (auto &data : group.tilingData) {
		data->tiling_depth = this->computeDataTilingDepth(root, group, *data);

		// create name helper for this tiling data
		data->nameHelper = std::make_unique<ptile::NameHelper>(data->tiling_depth, data->outer_for->iterator,
															   data->inner_for->iterator);
		data->nameHelper = std::make_unique<ptile::NameHelper>(data->tiling_depth, data->outer_for->iterator,
															   data->inner_for->iterator);
		data->trahrheNameHelper = std::make_unique<ptile::NameHelper>(data->tiling_depth, data->outer_for->iterator,
																	  data->inner_for->iterator);
		data->trahrheNameHelper->setPrefix("g" + std::to_string(group.id) + data->nameHelper->getSeparator() + "id" +
										   std::to_string(data->id));
	}

	// sort the tiling data by depth of the outer for
	std::sort(group.tilingData.begin(), group.tilingData.end(),
			  [](const std::unique_ptr<TilingData> &a, const std::unique_ptr<TilingData> &b) {
				  return a->tiling_depth < b->tiling_depth; // sort by depth of the outer for, ascending
			  });

	std::sort(group.tilingData.begin(), group.tilingData.end(),
			  [](const std::unique_ptr<TilingData> &a, const std::unique_ptr<TilingData> &b) {
				  return a->tiling_depth < b->tiling_depth;
			  });

	/* id must follow the sorted order, otherwise g<g>_id<n> has no relation to depth */
	for (size_t n = 0; n < group.tilingData.size(); n++) {
		group.tilingData[n]->id = n;
	}

	logger->debug("Tiling data for group {}:", group.id);
	for (auto &data : group.tilingData) {

		this->logger->debug("\ttiling data {}:  outer for: {}, inner for: {}, depth: {}", data->id,
							data->outer_for->iterator, data->inner_for->iterator, data->tiling_depth);
	}
}

void ClastParamTilingPass::pushTilingType(ParamTilingType type) {
	this->tilingTypes.push_back(type);
}

pesto::ParamTilingType ClastParamTilingPass::getTilingType(std::size_t depth) {
	// default is algebraic
	if (this->tilingTypes.size() == 0) {
		return ParamTilingType::ALGEBRAIC;
	}
	if (depth < this->tilingTypes.size()) {
		return this->tilingTypes[depth];
	}
	// if the depth is greater than the number of tiling types
	// we return the last one
	return this->tilingTypes[this->tilingTypes.size() - 1];
}

int ClastParamTilingPass::islFirstTilingDimIdx(StmtGroup &group) {
	int firstTilingDim = 1000;
	for (auto &data : group.tilingData) {
		int dim = isl_set_n_dim(isl_set_from_cloog_domain(data->outer_for->domain));
		firstTilingDim = dim < firstTilingDim ? dim : firstTilingDim;
	}
	return firstTilingDim - 1;
}

void ClastParamTilingPass::insertStatements(CloogOptions *cloogOptions, struct clast_root *root, TilingData &data) {
	struct clast_for *outer_for = data.outer_for;

	data.head_stmt_id = ++(this->insertedStmts);
	data.body_stmt_id = ++(this->insertedStmts);

	struct clast_user_stmt *header_stmt = pesto_clast_create_stmt(cloogOptions, NULL, data.head_stmt_id);
	clast_insert_before((struct clast_stmt *)root, (struct clast_stmt *)outer_for, (struct clast_stmt *)header_stmt);

	struct clast_user_stmt *body_stmt = pesto_clast_create_stmt(cloogOptions, NULL, data.body_stmt_id);
	clast_insert_indepth((struct clast_stmt *)outer_for, (struct clast_stmt *)body_stmt);
}

void ClastParamTilingPass::replaceInnerForBounds(CloogProgram *cloogProgram, StmtGroup &group, TilingData &data) {
	for (struct clast_for *inner_for : data.inner_fors) {

		free_clast_expr(inner_for->LB);
		free_clast_expr(inner_for->UB);
		inner_for->LB = NULL;
		inner_for->UB = NULL;

		isl_set *domain = isl_set_copy(isl_set_from_cloog_domain(inner_for->domain));
		domain = this->dropTilingConstraints(domain, group);

		isl_size ndim = isl_set_dim(domain, isl_dim_set);

		inner_for->LB = clast_bound_from_isl_set(cloogProgram->names, domain, 0, ndim - 1);
		inner_for->UB = clast_bound_from_isl_set(cloogProgram->names, domain, 1, ndim - 1);

		isl_set_free(domain);

		/* one fresh clast_name per sibling: clast nodes are owned by the tree
		 * and freed individually — sharing a pointer would double-free */
		auto lb_ptr = std::make_shared<std::string>(data.nameHelper->innerForLBName());
		struct clast_name *param_lb = new_clast_name(lb_ptr->c_str());
		this->names.push_back(lb_ptr);

		auto ub_ptr = std::make_shared<std::string>(data.nameHelper->innerForUBName());
		struct clast_name *param_ub = new_clast_name(ub_ptr->c_str());
		this->names.push_back(ub_ptr);

		if (inner_for->LB != NULL) {
			struct clast_reduction *red = new_clast_reduction(clast_red_max, 2);
			red->elts[0] = inner_for->LB;
			red->elts[1] = (struct clast_expr *)param_lb;
			inner_for->LB = (struct clast_expr *)red;
		} else {
			inner_for->LB = (struct clast_expr *)param_lb;
		}

		if (inner_for->UB != NULL) {
			struct clast_reduction *red = new_clast_reduction(clast_red_min, 2);
			red->elts[0] = inner_for->UB;
			red->elts[1] = (struct clast_expr *)param_ub;
			inner_for->UB = (struct clast_expr *)red;
		} else {
			inner_for->UB = (struct clast_expr *)param_ub;
		}
	}
}

void ClastParamTilingPass::replaceOuterForBounds(StmtGroup &group, TilingData &data) {
	(void)group;

	struct clast_for *outer_for = data.outer_for;

	free_clast_expr(outer_for->LB);
	free_clast_expr(outer_for->UB);
	outer_for->LB = NULL;
	outer_for->UB = NULL;

	cloog_int_t zero;
	cloog_int_init(zero);
	outer_for->LB = (struct clast_expr *)new_clast_term(zero, NULL);

	auto ub_ptr = std::make_shared<std::string>(data.nameHelper->outerForUBName());
	outer_for->UB = (struct clast_expr *)new_clast_name(ub_ptr->c_str());
	this->names.push_back(ub_ptr);
}

void ClastParamTilingPass::generateStatements(StmtGroup &group) {
	using StmtType = ptile::codegen::StmtType;

	for (auto &data : group.tilingData) {
		this->logger->debug("Generating statements... ({}, {})", data->head_stmt_id, data->body_stmt_id);
		ptile::codegen::ParamTilingCodegen codegen = this->getCodegen(group, *data);
		for (std::size_t i = 0; i < 2; ++i) {
			std::ostringstream oss;
			codegen.genStmt(oss, i == 0 ? StmtType::HEAD : StmtType::BODY);
			this->statements.push_back(
				std::make_pair(i == 0 ? data->head_stmt_id : data->body_stmt_id, std::string(oss.str())));
		}

		if (this->getTilingType(data->tiling_depth) == ParamTilingType::SCHEDULED &&
			this->getTilingType(data->tiling_depth + 1) == ParamTilingType::SCHEDULED) {
			codegen.setEndline("\n");
			std::ofstream ofs;
			ofs.open("schedule.h", std::ios::out);
			codegen.genScheduleHeaderFile(ofs);
			ofs.close();
		}
	}
}

isl_set *ClastParamTilingPass::dropTilingConstraints(__isl_take isl_set *set, StmtGroup &group) {
	isl_set *res = isl_set_copy(set);
	isl_set_free(set);

	// we need to drop the constraints that are not related to the tiling
	// dimensions
	// but from the inner most to the outer most
	// so we sort the tiling_for pair by descending depth of the outer for
	auto all = group.allTilingData();
	for (auto riter = all.rbegin(); riter != all.rend(); ++riter) {
		auto *data = *riter;

		int ndim = isl_set_dim(isl_set_from_cloog_domain(data->outer_for->domain), isl_dim_set);

		res = isl_set_drop_constraints_involving_dims(res, isl_dim_set, ndim - 1, 1);
	}

	return res;
}

void ClastParamTilingPass::declareVariable(std::string var) {
	// we do not want to declare the same variable twice
	if (std::find(this->varList.begin(), this->varList.end(), var) != this->varList.end()) {
		return;
	}
	this->varList.push_back(var);
}

void ClastParamTilingPass::declareGroupVariables(StmtGroup &group) {
	for (auto &data : group.tilingData) {
		auto &nameHelper = data->nameHelper;

		this->declareVariable(nameHelper->pcMaxName());
		this->declareVariable(nameHelper->targetVolName());
		this->declareVariable(nameHelper->outerForUBName());
		this->declareVariable(nameHelper->innerForLBName());
		this->declareVariable(nameHelper->innerForUBName());
	}
}

void ClastParamTilingPass::updateOmpPragma(StmtGroup &group) {
	// The parallel tile loop may be owned by another group (fused outer tile
	// loops): look at every enclosing level, and pick the outermost parallel one.
	TilingData *parallel_data = nullptr;
	for (auto *data : group.allTilingData()) {
		struct clast_for *outer_for = data->outer_for;
		logger->debug("group {} depth {}: outer_for '{}' parallel={}", group.id, data->tiling_depth,
					  outer_for->iterator ? outer_for->iterator : "?",
					  (outer_for->parallel & CLAST_PARALLEL_OMP) ? "yes" : "no");
		if ((outer_for->parallel & CLAST_PARALLEL_OMP) &&
			(parallel_data == nullptr || data->tiling_depth < parallel_data->tiling_depth)) {
			parallel_data = data;
		}
	}

	// we could be doing sequential tiling
	if (parallel_data == nullptr) {
		return;
	}

	struct clast_for *parallel_for = parallel_data->outer_for;
	int tiling_depth = parallel_data->tiling_depth;
	bool owned = std::any_of(group.tilingData.begin(), group.tilingData.end(),
							 [&](const std::unique_ptr<TilingData> &d) { return d.get() == parallel_data; });

	// The owner of the parallel level emits the directive; a borrowing group only
	// adds its own (deeper) variables to the private list of that same loop.
	if (owned) {
		parallel_for->parallel = CLAST_PARALLEL_OMP | CLAST_PARALLEL_USER;
		auto &nameHelper = parallel_data->nameHelper;

		std::string userDirectiveStr = "omp parallel for firstprivate(";
		userDirectiveStr += nameHelper->outerForUBName();
		if (this->getTilingType(tiling_depth) != ParamTilingType::RECTANGULAR) {
			userDirectiveStr += ", " + nameHelper->pcMaxName();
			userDirectiveStr += ", " + nameHelper->targetVolName();
		}
		userDirectiveStr += ") \\\n";

		parallel_for->user_directive = strdup(userDirectiveStr.c_str());
	}

	// private variables
	std::string privateVarsStr = parallel_for->private_vars ? std::string(parallel_for->private_vars) : "";
	free(parallel_for->private_vars);
	parallel_for->private_vars = nullptr;

	// Several groups can append to the same loop, and they share some names
	// (e.g. ub<outer it>, TARGET_VOL_L<depth>): OpenMP rejects duplicates.
	std::vector<std::string> present;
	{
		std::stringstream ss(privateVarsStr);
		std::string tok;
		while (std::getline(ss, tok, ',')) {
			tok.erase(0, tok.find_first_not_of(" \t"));
			tok.erase(tok.find_last_not_of(" \t") + 1);
			if (!tok.empty()) {
				present.push_back(tok);
			}
		}
	}
	auto addPrivate = [&](const std::string &var) {
		if (std::find(present.begin(), present.end(), var) != present.end()) {
			return;
		}
		present.push_back(var);
		if (!privateVarsStr.empty()) {
			privateVarsStr += ", ";
		}
		privateVarsStr += var;
	};

	if (owned) {
		addPrivate(parallel_data->nameHelper->innerForLBName());
		addPrivate(parallel_data->nameHelper->innerForUBName());

		if (this->getTilingType(tiling_depth) == ParamTilingType::SCHEDULED &&
			this->getTilingType(tiling_depth - 1) == ParamTilingType::SCHEDULED) {
			auto all = group.allTilingData();
			auto outerIt = std::find_if(all.begin(), all.end(),
										[&](TilingData *d) { return d->tiling_depth == tiling_depth - 1; });
			assert(outerIt != all.end());
			addPrivate((*outerIt)->nameHelper->innerForLBName());
			addPrivate((*outerIt)->nameHelper->innerForUBName());
		}
	}

	for (auto &data : group.tilingData) {
		if (data->tiling_depth <= tiling_depth) {
			continue;
		}
		auto &nameHelper = *data->nameHelper;
		addPrivate(nameHelper.outerForUBName());
		addPrivate(nameHelper.pcMaxName());
		addPrivate(nameHelper.targetVolName());
		addPrivate(nameHelper.innerForLBName());
		addPrivate(nameHelper.innerForUBName());
	}

	parallel_for->private_vars = strdup(privateVarsStr.c_str());
}

void ClastParamTilingPass::computeTrahrheDomain(StmtGroup &group) {
	int max_tiling_depth = 0;
	for (auto &data : group.tilingData) {
		max_tiling_depth = std::max(max_tiling_depth, data->tiling_depth);
	}

	isl_set *trahrheDomain = isl_set_copy(group.untiled_domain);

	// remove scalar dims
	isl_size ndim = isl_set_dim(trahrheDomain, isl_dim_set);
	for (isl_size i = 0; i < ndim; i++) {
		if (isl_set_is_dim_scalar(trahrheDomain, i)) {
			trahrheDomain = isl_set_remove_dims(trahrheDomain, isl_dim_set, i, 1);
			i -= 1;
			ndim = isl_set_dim(trahrheDomain, isl_dim_set);
		}
	}

	for (auto *itData : group.allTilingData()) {
		trahrheDomain = isl_set_set_dim_name(trahrheDomain, isl_dim_set, itData->tiling_depth,
											 itData->trahrheNameHelper->trahrheIteratorName().c_str());
	}

	isl_printer *printer = isl_printer_to_str(ctx);

	printer = isl_printer_print_set(printer, trahrheDomain);

	char *str = isl_printer_get_str(printer);

	logger->debug("Computed trahrhe domain for group {}, trahrhe depth {}:\n\t{}", group.id, max_tiling_depth + 1, str);

	this->trahrheDomainList_.push_back(std::make_pair(max_tiling_depth + 1, std::string(str)));

	free(str);
	isl_printer_free(printer);

	isl_set_free(trahrheDomain);
}

__isl_give isl_set *ClastParamTilingPass::removeTilingDimensions(StmtGroup &group, __isl_take isl_set *domain) {

	// assert(ndim <= group.tilingData.size());
	isl_ctx *ctx = isl_set_get_ctx(domain);
	isl_options_set_on_error(ctx, ISL_ON_ERROR_ABORT);

	// remove scalar dims
	for (int i = 0; i < isl_set_dim(domain, isl_dim_set); i++) {
		if (isl_set_is_dim_scalar(domain, i)) {
			domain = isl_set_remove_dims(domain, isl_dim_set, i, 1);
			i -= 1;
		}
	}

	// we are going to build a map
	// that will transform the current dimensions to keep only the tiled
	// dimensions in the order of the tiling iterators
	isl_space *space = isl_set_get_space(domain);
	isl_map *trans = isl_map_universe(space);

	// we move all dimensions to input dimensions
	trans = isl_map_move_dims(trans, isl_dim_in, 0, isl_dim_set, 0, isl_map_dim(trans, isl_dim_set));

	for (int i = 0; i < isl_map_dim(trans, isl_dim_in); i += 1) {
		int inner_idx = -1;
		bool should_skip = false;
		// look if this dimension corresponds to an inner tiling iterator
		for (auto *itData : group.allTilingData()) {
			int itOuterIdx = isl_map_find_dim_by_name(trans, isl_dim_in, itData->outer_for->iterator);
			int itInnerIdx = isl_map_find_dim_by_name(trans, isl_dim_in, group.innerFor(itData)->iterator);
			if (itInnerIdx != -1 && itOuterIdx == i) {
				inner_idx = itInnerIdx;
				should_skip = false;
				break;
			}
			if (itInnerIdx == i) {
				should_skip = true;
				break;
			}
		}

		if (should_skip) {
			continue;
		}
		// we add a new output dim
		trans = isl_map_add_dims(trans, isl_dim_out, 1);
		int out_idx = isl_map_dim(trans, isl_dim_out) - 1;

		isl_constraint *cst = isl_constraint_alloc_equality(isl_local_space_from_space(isl_map_get_space(trans)));
		cst = isl_constraint_set_coefficient_si(cst, isl_dim_in, inner_idx != -1 ? inner_idx : i, -1);
		cst = isl_constraint_set_coefficient_si(cst, isl_dim_out, out_idx, 1);

		trans = isl_map_add_constraint(trans, cst);

		trans = isl_map_set_dim_name(trans, isl_dim_out, out_idx,
									 isl_map_get_dim_name(trans, isl_dim_in, inner_idx != -1 ? inner_idx : i));
	}

	// for (auto &itData : group.tilingData) {
	// 	isl_constraint *cst = isl_constraint_alloc_equality(isl_local_space_from_space(isl_map_get_space(trans)));

	// 	int in_idx = isl_map_find_dim_by_name(trans, isl_dim_in, itData->inner_for->iterator);

	// 	cst = isl_constraint_set_coefficient_si(cst, isl_dim_in, in_idx, -1);
	// 	cst = isl_constraint_set_coefficient_si(cst, isl_dim_out, itData->tiling_depth, 1);

	// 	trans = isl_map_add_constraint(trans, cst);

	// 	trans = isl_map_set_dim_name(trans, isl_dim_out, itData->tiling_depth,
	// 								 isl_map_get_dim_name(trans, isl_dim_in, in_idx));
	// }

	assert(!isl_map_is_empty(trans));

	// apply the map to original domain
	isl_set *out = isl_set_apply(domain, trans);

	assert(!isl_set_is_empty(out));

	return out;
}

__isl_give isl_set *ClastParamTilingPass::moveNonTiledDimensionsToParameters(__isl_take isl_set *set,
																			 StmtGroup &group) {
	isl_set *domain = isl_set_copy(set);
	isl_set_free(set);
	size_t count = 0;

	isl_size ndim = isl_set_dim(domain, isl_dim_set);
	// TODO: move untiled dimensions as parameters
	for (isl_size i = 0; i < ndim; i++) {
		// ignore scalar dim
		if (isl_set_is_dim_scalar(domain, i)) {
			continue;
		}
		if (!isl_set_involves_dims(domain, isl_dim_set, i, 1)) {
			continue;
		}
		bool should_move = true;
		int innermost_outer_tiling_dim_idx = 0;
		// search if this dimension is tiled
		for (auto *it_data : group.allTilingData()) {
			int in_idx = -1;
			in_idx = isl_set_find_dim_by_name(domain, isl_dim_set, group.innerFor(it_data)->iterator);

			if (in_idx == i) {
				should_move = false;
				break;
			}
			int out_idx = isl_set_find_dim_by_name(domain, isl_dim_set, it_data->outer_for->iterator);
			innermost_outer_tiling_dim_idx = std::max(innermost_outer_tiling_dim_idx, out_idx);
			if (i == out_idx) {
				should_move = false;
				break;
			}
		}
		if (i > innermost_outer_tiling_dim_idx) {
			continue;
		}
		// if the dimension is tiled we keep it as it is
		if (!should_move) {
			continue;
		}

		this->logger->debug("dim {}({}) will be moved to param ", isl_set_get_dim_name(domain, isl_dim_set, i), i);
		isl_size npar = isl_set_dim(domain, isl_dim_param);
		domain = isl_set_move_dims(domain, isl_dim_param, npar, isl_dim_set, i, 1);
		i -= 1;
		ndim = isl_set_dim(domain, isl_dim_set);
		count += 1;
	}

	group.movedDimensionsCount = count;
	return domain;
}

pesto::ptile::codegen::ParamTilingCodegen ClastParamTilingPass::getCodegen(StmtGroup &group, TilingData &data) {
	ptile::codegen::ParamTilingCodegen codegen(group, data, this->id, isl_set_copy(group.untiled_domain));
	codegen.setBoundComputeMethod(this->boundComputeMethod);
	codegen.setBoundCorrectionStrategy(this->boundCorrectionStrategy);
	codegen.setTilingType(this->getTilingType(data.tiling_depth));
	if (this->volumeCheckThreshold.has_value() && !this->volumeCheckThreshold.value().empty()) {
		codegen.setVolumeCheckThreshold(this->volumeCheckThreshold.value());
	}

	if (this->volumeCheckTrigger == VolumeCheckTrigger::ALWAYS) {
		codegen.setVolumeCheckStrategy(this->volumeCheckStrategy);
	} else if (this->volumeCheckTrigger == VolumeCheckTrigger::FIRST_OUTER_ITERATOR_DEPENDENCY) {
		size_t movedDimCount = group.movedDimensionsCount;
		this->logger->debug("Moved dimensions count: {}", movedDimCount);
		if (movedDimCount == 0) {
			codegen.setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy::NO_CHECK);
		} else {
			bool isFirstDependency = true;
			for (auto &tdata : group.tilingData) {
				if (tdata->tiling_depth >= data.tiling_depth) {
					continue;
				}
				size_t iMovedDimCount = group.movedDimensionsCount;
				if (iMovedDimCount > 0) {
					isFirstDependency = false;
					break;
				}
			}
			if (isFirstDependency) {
				codegen.setVolumeCheckStrategy(this->volumeCheckStrategy);
				this->logger->debug("Volume check strategy triggered !");
			} else {
				codegen.setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy::NO_CHECK);
				this->logger->debug("Volume check strategy not triggered !");
			}
		}
	}
	return codegen;
}

void ClastParamTilingPass::clearTilingTypes() {
	this->tilingTypes.clear();
}

void ClastParamTilingPass::computeTilingDataBoundExpr(CloogProgram *cloogProgram, StmtGroup &group, TilingData &data) {
	using ptile::NameHelper;
	(void)cloogProgram;

	isl_set *domain = isl_set_copy(group.untiled_domain);

	logger->debug("computeTilingDataBoundExpr: group {} id {} depth {} outer '{}' inner '{}'", group.id, data.id,
				  data.tiling_depth, data.outer_for->iterator, data.inner_for->iterator);

	/* Enclosing tiled dimensions are confined to their tile: lbX <= tX <= ubX.
	 * Add lbX/ubX as PARAMETERS, constrain tX, then PROJECT tX out.
	 *
	 * Do NOT simply rename tX to lbX for the lower bound and to ubX for the
	 * upper bound: that silently assumes tX appears with a positive coefficient.
	 * knuth has "t7 >= 2 - t5" (negative coefficient), where the minimum of t7
	 * over the tile is reached at t5 = ubt5, not lbt5. Fourier-Motzkin picks the
	 * right extremum from the sign; a rename cannot. */
	int myDim = isl_set_find_dim_by_name(domain, isl_dim_set, data.inner_for->iterator);
	if (myDim < 0) {
		logger->error("computeTilingDataBoundExpr: dim '{}' not found in untiled domain", data.inner_for->iterator);
		isl_set_free(domain);
		return;
	}

	for (isl_size i = isl_set_dim(domain, isl_dim_set) - 1; i >= 0; i -= 1) {

		if (i == myDim) {
			continue; /* the dimension we are computing bounds for */
		}

		if (i < myDim) {
			/* Enclosing dimension: it is confined to its tile. Add lbX <= tX <= ubX
			 * as parameters BEFORE eliminating tX, so our bounds are expressed in
			 * terms of the tile, not of tX's full range.
			 *
			 * Do NOT rename tX to lbX (resp. ubX) for the lower (resp. upper) bound:
			 * that silently assumes tX has a positive coefficient. knuth has
			 * "t7 >= 2 - t5", where min(t7) over the tile is reached at t5 = ubt5.
			 * Fourier-Motzkin picks the right extremum from the sign; a rename cannot. */
			auto all = group.allTilingData();
			auto it = std::find_if(all.begin(), all.end(), [&](const TilingData *d) {
				return isl_set_find_dim_by_name(domain, isl_dim_set, group.innerFor(d)->iterator) == i;
			});

			if (it != all.end()) {
				const std::string lbName = (*it)->nameHelper->innerForLBName();
				const std::string ubName = (*it)->nameHelper->innerForUBName();

				isl_size nparam = isl_set_dim(domain, isl_dim_param);
				domain = isl_set_add_dims(domain, isl_dim_param, 2);
				domain = isl_set_set_dim_name(domain, isl_dim_param, nparam, lbName.c_str());
				domain = isl_set_set_dim_name(domain, isl_dim_param, nparam + 1, ubName.c_str());

				isl_local_space *ls = isl_local_space_from_space(isl_set_get_space(domain));

				/* tX - lbX >= 0 */
				isl_constraint *cLb = isl_constraint_alloc_inequality(isl_local_space_copy(ls));
				cLb = isl_constraint_set_coefficient_si(cLb, isl_dim_set, i, 1);
				cLb = isl_constraint_set_coefficient_si(cLb, isl_dim_param, nparam, -1);
				domain = isl_set_add_constraint(domain, cLb);

				/* ubX - tX >= 0 */
				isl_constraint *cUb = isl_constraint_alloc_inequality(isl_local_space_copy(ls));
				cUb = isl_constraint_set_coefficient_si(cUb, isl_dim_set, i, -1);
				cUb = isl_constraint_set_coefficient_si(cUb, isl_dim_param, nparam + 1, 1);
				domain = isl_set_add_constraint(domain, cUb);

				isl_local_space_free(ls);
			}
		}

		/* Every other dimension goes too — inner tiled ones AND untiled ones (t9).
		 * Leaving them in lets isl express our bounds in terms of them. */
		domain = isl_set_project_out(domain, isl_dim_set, i, 1);
		if (i < myDim) {
			myDim -= 1;
		}
	}

	isl_printer *printer = isl_printer_to_str(this->ctx);
	printer = isl_printer_set_output_format(printer, ISL_FORMAT_C);

	isl_pw_aff *lb = isl_set_dim_lower_bound(isl_set_copy(domain), myDim);
	isl_pw_aff *ub = isl_set_dim_upper_bound(isl_set_copy(domain), myDim);

	printer = isl_printer_print_pw_aff(printer, lb);
	char *lbStr = isl_printer_get_str(printer);
	printer = isl_printer_flush(printer);

	printer = isl_printer_print_pw_aff(printer, ub);
	char *ubStr = isl_printer_get_str(printer);
	printer = isl_printer_flush(printer);

	data.innerForLBExpr = std::string(lbStr);
	data.innerForUBExpr = std::string(ubStr);

	logger->debug("  '{}' bounds: LB = {}   UB = {}", data.inner_for->iterator, data.innerForLBExpr,
				  data.innerForUBExpr);

	free(lbStr);
	free(ubStr);
	isl_pw_aff_free(lb);
	isl_pw_aff_free(ub);
	isl_printer_free(printer);
	isl_set_free(domain);
}

int ClastParamTilingPass::computeDataTilingDepth(struct clast_root *root, StmtGroup &group, TilingData &data) {
	int depth = 0;

	auto countIfEncloses = [&](TilingData *itData) {
		if (itData->outer_for == data.outer_for) {
			return;
		}
		/* rooted at the CLAST root, not at group.outermost_for: a borrowed tiling
		 * dimension lives outside this group's subtree and would never be found. */
		if (clast_is_parent_clast_for(&(root->stmt), itData->outer_for, data.outer_for)) {
			depth += 1;
		}
	};

	for (auto &itData : group.tilingData) {
		countIfEncloses(itData.get());
	}
	for (auto *itData : group.borrowedTilingData) {
		countIfEncloses(itData);
	}

	return depth;
}

void ClastParamTilingPass::computeUntiledDomain(CloogProgram *cloogProgram, StmtGroup &group) {
	logger->debug("Computing untiled domain for group {}", group.id);

	isl_set *inner_domain = isl_set_copy(isl_set_from_cloog_domain(group.stmt_parent_for->domain));

	assert(inner_domain != nullptr);

	// define param names
	for (int i = 0; i < cloogProgram->names->nb_parameters; i += 1) {
		inner_domain = isl_set_set_dim_name(inner_domain, isl_dim_param, i, cloogProgram->names->parameters[i]);
	}

	for (isl_size i = 0; i < isl_set_n_dim(inner_domain); i += 1) {
		inner_domain = isl_set_set_dim_name(inner_domain, isl_dim_set, i, cloogProgram->names->scattering[i]);
	}

	// move non tiled dimensions to parameters
	inner_domain = this->moveNonTiledDimensionsToParameters(inner_domain, group);

	inner_domain = this->removeTilingDimensions(group, inner_domain);

	assert(inner_domain != nullptr || "inner domain is null");
	assert(!isl_set_is_empty(inner_domain));

	if (logger->level() <= spdlog::level::debug) {
		isl_printer *printer = isl_printer_to_str(ctx);
		printer = isl_printer_print_set(printer, inner_domain);
		char *str = isl_printer_get_str(printer);
		logger->debug("Untiled domain of group {}:\n{}", group.id, str);
		isl_printer_free(printer);
		free(str);
	}

	group.untiled_domain = inner_domain;
}

void ClastParamTilingPass::setGenClastPass(std::unique_ptr<GenClastPass> pass) {
	this->gen_clast_pass = std::move(pass);
}

pesto::StmtGroup::~StmtGroup() {
	if (this->untiled_domain != nullptr) {
		isl_set_free(this->untiled_domain);
	}
}
