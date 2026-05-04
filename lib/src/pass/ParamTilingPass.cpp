/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <algorithm>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <cloog/isl/cloog.h>

#include <osl/osl.h>

#include <isl/ctx.h>
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
#include <pesto/pass/CodegenPasses.hpp>
#include <pesto/pass/Pass.hpp>
#include <pesto/pass/PlutoPass.hpp>
#include <pesto/pass/clast/ParamTilingPass.hpp>

using pesto::Bundle;
using pesto::pass::ClastParamTilingPass;

pesto::pass::ClastParamTilingPass::ClastParamTilingPass()
	: IPass("param-tiling"), gen_trahrhe(false), trahrhe_use_dichotomy(true), tilingTypes() {
	this->gen_clast_pass = std::make_unique<GenClastPass>();
	this->gen_clast_pass->cloogF = -1;
	this->gen_clast_pass->cloogL = 1;
	// TODO: WHY IS THIS HERE?
	// This should be set in a separate pass
	this->gen_clast_pass->codegenContext = 1;

	this->ctx = isl_ctx_alloc();
}

pesto::pass::ClastParamTilingPass::~ClastParamTilingPass() {
	isl_ctx_free(this->ctx);
}

std::set<std::string> pesto::pass::ClastParamTilingPass::inputIR() const {
	return {pesto::ir::OpenScopIR::ID};
}

std::string pesto::pass::ClastParamTilingPass::outputIR() const {
	return pesto::ir::ClastIR::ID;
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
	if (bundle.get_ir()->id() != pesto::ir::ClastIR::ID) {
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
		// First we need to associate for each tiled dimension its inner and outter
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

			FILE *fp = fopen(header_path.c_str(), "w");
			if (fp == NULL) {
				logger->error("Error: Could not open file {}", header_filename.c_str());
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
			fclose(fp);
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
	struct clast_user_stmt **stmt = clast_get_user_stmt_list((struct clast_stmt *)root, &stmt_count);
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

	// sort by clast depth
	// deeper last
	std::sort(stmt_to_process.begin(), stmt_to_process.end(),
			  [root](struct clast_user_stmt *a, struct clast_user_stmt *b) {
				  struct clast_for *a_parent_for = clast_get_parent_clast_for(&(root->stmt), &(a->stmt));
				  struct clast_for *b_parent_for = clast_get_parent_clast_for(&(root->stmt), &(b->stmt));

				  return clast_for_depth(&(root->stmt), a_parent_for) < clast_for_depth(&(root->stmt), b_parent_for);
			  });

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

	// Then for each outter for (from outter most to inner most)
	for (auto &data : group.tilingData) {
		logger->debug("Tiling for {} with outer iterator {} at depth {} ", data->id, data->outer_for->iterator,
					  data->tiling_depth);

		this->insertStatements(cloogOptions, root, *data);
		this->replaceOutterForBounds(group, *data);

		this->replaceInnerForBounds(cloogProgram, group, *data);
	}
	this->computeTrahrheDomain(cloogProgram, group);

	logger->debug("Generating stmts for group {}", group.id);
	this->generateStatements(cloogProgram, group);
	this->declareGroupVariables(group);

	// then we need to handle the parallel loop and update openmp pragma
	this->updateOmpPragma(group);

	isl_set_free(group_domain);
}

void ClastParamTilingPass::associateTilingFor(struct clast_root *root, StmtGroup &group) {
	for (auto *stmt : group.stmts) {
		for (struct clast_for *current_for = clast_get_parent_clast_for(&(root->stmt), &(stmt->stmt));
			 current_for != NULL; current_for = clast_get_parent_clast_for(&(root->stmt), &(current_for->stmt))) {
			struct clast_for *outer_for = clast_find_outter_tiling_for(&(root->stmt), current_for);

			for (auto &data : group.tilingData) {
				if (data->outer_for == outer_for || data->inner_for == current_for) {
					// this for is already associated to a tiling data, we skip it
					outer_for = nullptr;
					break;
				}
			}

			if (outer_for == nullptr) {
				continue;
			}

			auto data = std::make_unique<TilingData>();
			data->id = group.tilingData.size();
			data->outer_for = outer_for;
			data->inner_for = current_for;

			data->tiling_depth = this->computeDataTilingDepth(group, *data);

			group.tilingData.push_back(std::move(data));
		}
	}

	// compute tiling depth for each tiling data
	for (auto &data : group.tilingData) {
		data->tiling_depth = this->computeDataTilingDepth(group, *data);

		// create name helper for this tiling data
		data->nameHelper = std::make_unique<ptile::NameHelper>(data->tiling_depth, data->outer_for->iterator,
															   data->inner_for->iterator);
		data->nameHelper->setPrefix("g" + std::to_string(group.id) + "_id" + std::to_string(data->id));

		data->trahrheNameHelper = std::make_unique<ptile::NameHelper>(data->tiling_depth, data->outer_for->iterator,
																	  data->inner_for->iterator);
		data->trahrheNameHelper->setPrefix("g" + std::to_string(group.id) + data->nameHelper->getSeparator() + "id" +
										   std::to_string(data->id));
	}

	// sort the tiling data by depth of the outter for
	std::sort(group.tilingData.begin(), group.tilingData.end(),
			  [](const std::unique_ptr<TilingData> &a, const std::unique_ptr<TilingData> &b) {
				  return a->tiling_depth < b->tiling_depth; // sort by depth of the outter for, ascending
			  });

	logger->debug("Tiling data for group {}:", group.id);
	for (auto &data : group.tilingData) {

		this->logger->debug("\ttiling data {}:  outter for: {}, inner for: {}, depth: {}", data->id,
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
	struct clast_for *outter_for = data.outer_for;

	data.head_stmt_id = ++(this->insertedStmts);
	data.body_stmt_id = ++(this->insertedStmts);

	struct clast_user_stmt *header_stmt = pesto_clast_create_stmt(cloogOptions, NULL, data.head_stmt_id);
	clast_insert_before((struct clast_stmt *)root, (struct clast_stmt *)outter_for, (struct clast_stmt *)header_stmt);

	struct clast_user_stmt *body_stmt = pesto_clast_create_stmt(cloogOptions, NULL, data.body_stmt_id);
	clast_insert_indepth((struct clast_stmt *)outter_for, (struct clast_stmt *)body_stmt);
}

void ClastParamTilingPass::replaceInnerForBounds(CloogProgram *cloogProgram, StmtGroup &group, TilingData &data) {
	struct clast_for *inner_for = data.inner_for;

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

	auto lb_ptr = std::make_shared<std::string>(data.nameHelper->innerForLBName());
	struct clast_name *param_lb = new_clast_name(lb_ptr->c_str());
	this->names.push_back(lb_ptr);

	auto ub_ptr = std::make_shared<std::string>(data.nameHelper->innerForUBName());
	struct clast_name *param_ub = new_clast_name(ub_ptr->c_str());
	this->names.push_back(ub_ptr);

	if (inner_for->LB != NULL) {
		struct clast_reduction *red = NULL;
		red = new_clast_reduction(clast_red_max, 2);
		red->elts[0] = inner_for->LB;
		red->elts[1] = (struct clast_expr *)param_lb;
		inner_for->LB = (struct clast_expr *)red;
	} else {
		inner_for->LB = (struct clast_expr *)param_lb;
	}

	if (inner_for->UB != NULL) {
		struct clast_reduction *red = NULL;
		red = new_clast_reduction(clast_red_min, 2);
		red->elts[0] = inner_for->UB;
		red->elts[1] = (struct clast_expr *)param_ub;
		inner_for->UB = (struct clast_expr *)red;
	} else {
		inner_for->UB = (struct clast_expr *)param_ub;
	}
}

void ClastParamTilingPass::replaceOutterForBounds(StmtGroup &group, TilingData &data) {
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

void ClastParamTilingPass::generateStatements(CloogProgram *cloogProgram, StmtGroup &group) {
	using StmtType = ptile::codegen::StmtType;

	for (auto &data : group.tilingData) {
		ptile::codegen::ParamTilingCodegen codegen = this->getCodegen(cloogProgram, group, *data);
		this->logger->debug("Generating statements... ({}, {})", data->head_stmt_id, data->body_stmt_id);
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
	// but from the inner most to the outter most
	// so we sort the tiling_for pair by descending depth of the outter for
	for (auto riter = group.tilingData.rbegin(); riter != group.tilingData.rend(); ++riter) {
		auto &data = *riter;

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
	// first find the parallel for
	struct clast_for *parallel_for = nullptr;
	int tiling_depth = -1;
	for (auto &data : group.tilingData) {
		struct clast_for *outter_for = data->outer_for;
		if (outter_for->parallel & CLAST_PARALLEL_OMP) {
			parallel_for = outter_for;
			tiling_depth = data->tiling_depth;
			break;
		}
	}

	// we could be doing sequential tiling
	if (parallel_for == nullptr) {
		return;
	}

	// then we need to update the pragma so it can handle custom directives
	parallel_for->parallel = CLAST_PARALLEL_OMP | CLAST_PARALLEL_USER;
	std::string userDirectiveStr = "omp parallel for firstprivate(";

	auto &nameHelper = group.tilingData[0]->nameHelper;

	// omp firstprivate directive
	userDirectiveStr += nameHelper->outerForUBName() + ", ";
	userDirectiveStr += nameHelper->pcMaxName() + ", ";
	userDirectiveStr += nameHelper->targetVolName() + ") \\\n";

	parallel_for->user_directive = strdup(userDirectiveStr.c_str());

	// private variables
	std::string privateVarsStr = std::string(parallel_for->private_vars);

	// now we can free old private vars
	free(parallel_for->private_vars);
	parallel_for->private_vars = nullptr;

	if (privateVarsStr != "") {
		privateVarsStr += ", ";
	}
	auto &parallelNameHelper = group.tilingData[tiling_depth]->nameHelper;

	privateVarsStr += parallelNameHelper->innerForLBName();
	privateVarsStr += ", " + parallelNameHelper->innerForUBName();

	if (this->getTilingType(tiling_depth) == ParamTilingType::SCHEDULED &&
		this->getTilingType(tiling_depth - 1) == ParamTilingType::SCHEDULED) {
		auto &outerHelper = group.tilingData[tiling_depth - 1]->nameHelper;
		privateVarsStr += ", " + outerHelper->innerForLBName();
		privateVarsStr += ", " + outerHelper->innerForUBName();
	}

	for (auto &data : group.tilingData) {
		if (data->tiling_depth <= tiling_depth) {
			continue;
		}
		auto &nameHelper = *data->nameHelper;
		privateVarsStr += ", " + nameHelper.outerForUBName();
		privateVarsStr += ", " + nameHelper.pcMaxName();
		privateVarsStr += ", " + nameHelper.targetVolName();
		privateVarsStr += ", " + nameHelper.innerForLBName();
		privateVarsStr += ", " + nameHelper.innerForUBName();
	}

	parallel_for->private_vars = strdup(privateVarsStr.c_str());
}

void ClastParamTilingPass::computeTrahrheDomain(CloogProgram *cloogProgram, StmtGroup &group) {
	int max_tiling_depth = 0;
	for (auto &data : group.tilingData) {
		max_tiling_depth = std::max(max_tiling_depth, data->tiling_depth);
	}

	isl_set *trahrheDomain = isl_set_copy(group.untiled_domain);

	// // define param names
	// for (int i = 0; i < cloogProgram->names->nb_parameters; i += 1) {
	// 	trahrheDomain = isl_set_set_dim_name(trahrheDomain, isl_dim_param, i, cloogProgram->names->parameters[i]);
	// }

	// for (isl_size i = 0; i < isl_set_n_dim(trahrheDomain); i += 1) {
	// 	trahrheDomain = isl_set_set_dim_name(trahrheDomain, isl_dim_set, i, cloogProgram->names->scattering[i]);
	// }

	isl_set_dump(trahrheDomain);

	// remove scalar dims
	isl_size ndim = isl_set_dim(trahrheDomain, isl_dim_set);
	for (isl_size i = 0; i < ndim; i++) {
		if (isl_set_is_dim_scalar(trahrheDomain, i)) {
			trahrheDomain = isl_set_remove_dims(trahrheDomain, isl_dim_set, i, 1);
			i -= 1;
			ndim = isl_set_dim(trahrheDomain, isl_dim_set);
		}
	}

	// trahrheDomain = this->moveNonTiledDimensionsToParameters(trahrheDomain, data, group);

	// trahrheDomain = this->removeTilingDimensions(group, trahrheDomain, -1);

	for (auto &itData : group.tilingData) {
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

__isl_give isl_set *ClastParamTilingPass::removeTilingDimensions(StmtGroup &group, __isl_take isl_set *domain,
																 int maxTilingDepth) {
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
		for (auto &itData : group.tilingData) {
			int itOuterIdx = isl_map_find_dim_by_name(trans, isl_dim_in, itData->outer_for->iterator);
			int itInnerIdx = isl_map_find_dim_by_name(trans, isl_dim_in, itData->inner_for->iterator);
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
		for (auto &it_data : group.tilingData) {
			int in_idx = -1;
			in_idx = isl_set_find_dim_by_name(domain, isl_dim_set, it_data->inner_for->iterator);

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

pesto::ptile::codegen::ParamTilingCodegen ClastParamTilingPass::getCodegen(CloogProgram *cloogProgram, StmtGroup &group,
																		   TilingData &data) {
	ptile::codegen::ParamTilingCodegen codegen(group, data, this->id, isl_set_copy(group.untiled_domain));
	codegen.setBoundComputeMethod(this->boundComputeMethod);
	codegen.setBoundCorrectionStrategy(this->boundCorrectionStrategy);
	codegen.setTilingType(this->getTilingType(data.tiling_depth));
	if (this->volumeCheckThreshold.has_value() && !this->volumeCheckThreshold.value().empty()) {
		codegen.setVolumeCheckThreshold(this->volumeCheckThreshold.value());
	}

	if (this->volumeCheckTrigger == VolumeCheckTrigger::ALWAYS) {
		codegen.setVolumeCheckStrategy(this->volumeCheckStrategy);
	} else if (this->volumeCheckTrigger == VolumeCheckTrigger::FIRST_OUTTER_ITERATOR_DEPENDENCY) {
		size_t movedDimCount = group.movedDimensionsCount;
		if (movedDimCount == 0) {
			codegen.setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy::NO_CHECK);
		} else {
			this->logger->debug("Moved dimensions count: {}", movedDimCount);
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

	logger->debug("Computing for loop bounds for tiling data {} of group {} ", data.id, group.id);

	// compute loop bound expr
	std::string outterForIt = std::string(data.outer_for->iterator);
	isl_set *innerDomain = isl_set_copy(group.untiled_domain);

	// [0] is for lower bound, [1] is for upper bound
	CloogOptions *cloogOptions = cloog_options_malloc(nullptr);
	CloogNames *namesList[2] = {nullptr, nullptr};
	isl_size npar = isl_set_dim(innerDomain, isl_dim_param);
	for (std::size_t i = 0; i < 2; ++i) {
		namesList[i] = cloog_names_malloc();
		if (namesList[i] == NULL) {
			this->logger->error("Cannot allocate memory for cloog names");
			break;
		}
		CloogNames *names = namesList[i];
		names->nb_parameters = npar;
		names->parameters = (char **)calloc(npar, sizeof(char *));
		for (isl_size j = 0; j < npar; j += 1) {
			names->parameters[j] = strdup(isl_set_get_dim_name(innerDomain, isl_dim_param, j));
		}
		names->nb_scattering = group.tilingData.size();
		names->scattering = (char **)calloc(names->nb_scattering, sizeof(char *));
		for (isl_size j = 0; j < names->nb_scattering; j += 1) {
			auto &data = group.tilingData[j];
			std::string bound = i == 0 ? data->nameHelper->innerForLBName() : data->nameHelper->innerForUBName();
			names->scattering[j] = strdup(bound.c_str());
		}
	}

	// project dimensions that are after the current dimensions to avoid dependency on them (as they won't be computed
	// yet when we compute the bound for the current dimension)
	int ndim = isl_set_dim(innerDomain, isl_dim_set);
	if (data.tiling_depth + 1 < ndim) {
		innerDomain =
			isl_set_project_out(innerDomain, isl_dim_set, data.tiling_depth + 1, ndim - data.tiling_depth - 1);
	}

	char *buffer = (char *)calloc(512, sizeof(char));
	FILE *fd = fmemopen(buffer, 512, "w");

	for (std::size_t i = 0; i < 2; ++i) {
		struct clast_expr *expr = clast_bound_from_isl_set(namesList[i], innerDomain, i, data.tiling_depth);
		if (expr != NULL) {
			memset(buffer, 0, 512);
			rewind(fd);
			clast_pprint_expr(cloogOptions, fd, expr);
			fflush(fd);
			if (i == 0) {
				data.innerForLBExpr = std::string(buffer);
			} else {
				data.innerForUBExpr = std::string(buffer);
			}
		}
		free_clast_expr(expr);
	}
	fclose(fd);
	free(buffer);

	for (std::size_t i = 0; i < 2; ++i) {
		cloog_names_free(namesList[i]);
	}
	cloog_options_free(cloogOptions);

	isl_set_free(innerDomain);
}

int ClastParamTilingPass::computeDataTilingDepth(StmtGroup &group, TilingData &data) {
	int depth = 0;
	for (auto &itData : group.tilingData) {
		if (itData->outer_for == data.outer_for) {
			continue;
		}
		if (clast_is_parent_clast_for(&(group.outermost_for->stmt), itData->outer_for, data.outer_for)) {
			depth += 1;
		}
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

	isl_set_dump(inner_domain);

	// move non tiled dimensions to parameters
	inner_domain = this->moveNonTiledDimensionsToParameters(inner_domain, group);

	logger->debug("After moving non-tiled dimensions to parameters:");
	isl_set_dump(inner_domain);

	inner_domain = this->removeTilingDimensions(group, inner_domain, -1);

	logger->debug("After removing tiling dimensions:");
	isl_set_dump(inner_domain);

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

pesto::StmtGroup::~StmtGroup() {
	if (this->untiled_domain != nullptr) {
		isl_set_free(this->untiled_domain);
	}
}