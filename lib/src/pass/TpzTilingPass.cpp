/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <vector>

#include <isl/aff.h>
#include <isl/ast_build.h>
#include <isl/flow.h>
#include <isl/maybe_pw_aff.h>
#include <isl/multi.h>
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
#include <pesto/macros.h>
#include <pesto/maths/Maths.hpp>
#include <pesto/metadata/FileInfo.hpp>
#include <pesto/metadata/Macro.hpp>
#include <pesto/metadata/VarDecl.hpp>
#include <pesto/pass/CodegenPasses.hpp>
#include <pesto/pass/OslPass.hpp>
#include <pesto/pass/PlutoPass.hpp>
#include <pesto/pass/TpzTilingPass.hpp>

using pesto::Bundle;
using pesto::pass::TpzTilingPass;
using pesto::pass::ptile::ClastTilingPair;

TpzTilingPass::TpzTilingPass() : IPass("tpz-tiling") {
	this->ctx = isl_ctx_alloc();
	this->printer = isl_printer_to_str(this->ctx);
}

TpzTilingPass::~TpzTilingPass() {
	isl_printer_free(this->printer);
	isl_ctx_free(this->ctx);
}

namespace pesto::pass {} // namespace pesto::pass

Bundle TpzTilingPass::apply(Bundle &&bundle) {
	auto *scopIR = dynamic_cast<pesto::ir::OpenScopIR *>(bundle.get_ir());
	osl_scop_p scop = scopIR->c_scop();

	std::optional<pesto::maths::Vec2i> slope = this->computeDepDistanceVector(scop);

	if (!slope.has_value()) {
		this->logger->error("Trapezoidal tiling cannot be performed on this scop.");
		return std::move(bundle);
	}

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
	if (bundle.get_ir()->id() != pesto::ir::ClastIR::ID) {
		logger->error("Expected ClastIR after GenClastPass, got {}", bundle.get_ir()->id().c_str());
		throw std::runtime_error("Unexpected IR after GenClastPass");
	}
	auto *clastIR = dynamic_cast<pesto::ir::ClastIR *>(bundle.get_ir());
	struct clast_root *root = (struct clast_root *)clastIR->get();

	auto stmts = clast_get_user_stmt_list(&(root->stmt), &this->insertedStmts);
	free(stmts);

	// TODO: better alogorithm to find the for loop
	// for now we just take the first for loop in ast
	for (struct clast_stmt *stmt = &(root->stmt); stmt != NULL; stmt = clast_get_next(stmt)) {
		if (CLAST_STMT_IS_A(stmt, stmt_for)) {
			struct clast_for *clastFor = (struct clast_for *)stmt;

			tileClastFor(bundle, clastFor, slope.value());
			break;
		}
	}

	return std::move(bundle);
}

void TpzTilingPass::tileClastFor(Bundle &bundle, struct clast_for *clastFor, pesto::maths::Vec2i slope) {
	using pesto::pass::ptile::ClastTilingPair;
	using namespace pesto::bundle;

	assert(bundle.get_ir()->id() == pesto::ir::ClastIR::ID);
	auto *clastIR = dynamic_cast<pesto::ir::ClastIR *>(bundle.get_ir());

	struct clast_root *root = (struct clast_root *)clastIR->get();

	auto &macroData = bundle.metadata.get<metadata::Macro>();

	this->logger->debug("tiling clastFor: {}", clastFor->iterator);
	std::vector<ClastTilingPair> clastTilingPairs = this->computeTilingPairs(*clastIR, clastFor);

	isl_set *innerDomain = this->computeInnerDomain(*clastIR, clastTilingPairs);

	if (logger->level() <= spdlog::level::debug) {
		this->printer = isl_printer_print_set(this->printer, innerDomain);
		char *set_str = isl_printer_get_str(printer);
		logger->debug("Computed inner domain: {}", set_str);
		free(set_str);
		this->printer = isl_printer_flush(this->printer);
	}

	this->insertOuterForLoop(bundle, clastTilingPairs);

	// duplicate ast at depth 1
	ClastTilingPair &trapezoidalPair = clastTilingPairs[1];

	this->logger->debug("Duplicating outer for loop {}", trapezoidalPair.outerFor->iterator);
	// TODO: we might have to find a way to group user statement by pair of original and duplicated
	struct clast_stmt *secondTileBand = clast_stmt_hard_copy(&(trapezoidalPair.outerFor->stmt));
	trapezoidalPair.outerFor->stmt.next = secondTileBand;
	this->logger->debug("Duplication done");

	// replace the duplicated inner for user statements

	int dupClastUserStmtListSize = 0;
	clast_user_stmt **dupUserStmtList = clast_get_user_stmt_list(secondTileBand, &dupClastUserStmtListSize);

	for (size_t i = 0; i < clastTilingPairs.size(); ++i) {
		ClastTilingPair &pair = clastTilingPairs[i];
		if (pair.innerBodyStmt0 == nullptr) {
			continue;
		}
		int targetStmtNumber = pair.innerBodyStmt0->statement->number; // -1 because we start counting at 1
		clast_user_stmt *targetStmt = nullptr;
		;
		for (int j = 0; j < dupClastUserStmtListSize; ++j) {
			clast_user_stmt *dupStmt = dupUserStmtList[j];
			if (dupStmt->statement->number == targetStmtNumber) {
				targetStmt = dupStmt;
				break;
			}
		}
		if (targetStmt == nullptr) {
			this->logger->error("Error: could not find duplicated user statement for number {}", targetStmtNumber);
			continue;
		}
		this->insertedStmts += 1;							 // Increment the inserted statement count
		targetStmt->statement->number = this->insertedStmts; // Update the statement number

		pair.innerBodyStmt1 = targetStmt;
	}

	free(dupUserStmtList);

	this->insertInnerLoopBounds(*clastIR, clastTilingPairs);
	this->insertOmpPragmas(*clastIR, clastTilingPairs);
	clast_root_develop_user_stmt_substitutions(root);

	this->codegen(bundle, clastTilingPairs, innerDomain, slope);

	if (logger->level() <= spdlog::level::debug) {
		this->printer = isl_printer_print_set(this->printer, innerDomain);
		char *set_str = isl_printer_get_str(printer);
		logger->debug("Starting trahrhe header generation using domain: {}", set_str);
		free(set_str);
		this->printer = isl_printer_flush(this->printer);
	}
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

	// init tiling pairs with inner for loops
	for (struct clast_for *for_it = clast_get_innermost_for(&(clastFor->stmt)); for_it != NULL;
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

	return clastTilingPairs;
}

void TpzTilingPass::insertOuterForLoop(Bundle &bundle, std::vector<ptile::ClastTilingPair> &clastTilingPairs) {
	using pesto::ptile::NameHelper;
	assert(bundle.get_ir()->id() == pesto::ir::ClastIR::ID);
	auto *clastIR = dynamic_cast<pesto::ir::ClastIR *>(bundle.get_ir());

	struct clast_root *root = (struct clast_root *)clastIR->get();
	CloogOptions *cloogOptions = clastIR->getCloogOptions();
	auto &varDecl = bundle.metadata.get<pesto::bundle::metadata::VarDecl>();
	struct clast_for *outermostFor = clastTilingPairs[0].innerFor;

	CloogDomain *enclosingDomain =
		cloog_domain_universe(cloogOptions->state, cloog_domain_dimension(outermostFor->domain));

	// do tiling by inserting outer for loops
	// insert user statements
	for (auto it = clastTilingPairs.begin(); it != clastTilingPairs.end(); ++it) {
		// Access *it as a ClastTilingPair
		// Example: log(std::cerr) << "Processing pair with innerFor: " << it->innerFor->iterator << std::endl;
		struct clast_for *enclosing_for =
			clast_insert_enclosing_for(&(root->stmt), &(outermostFor->stmt), enclosingDomain);

		// Set iterator name
		std::unique_ptr<std::string> it_name = std::make_unique<std::string>("z" + std::string(it->innerFor->iterator));

		NameHelper nameHelper = NameHelper(it->depth, *it_name, it->innerFor->iterator);

		std::unique_ptr<std::string> lbName = std::make_unique<std::string>(nameHelper.outerForLBName());
		std::unique_ptr<std::string> ubName = std::make_unique<std::string>(nameHelper.outerForUBName());

		logger->debug("Creating outer for loop with iterator: {}", it_name->c_str());

		varDecl.addVar(*it_name);
		varDecl.addVar(nameHelper.outerForLBName());
		varDecl.addVar(nameHelper.outerForUBName());

		enclosing_for->iterator = it_name->c_str(); // Set the iterator name for the enclosing loop

		enclosing_for->LB = &(new_clast_name(lbName->c_str())->expr);
		enclosing_for->UB = &(new_clast_name(ubName->c_str())->expr);

		it->outerFor = enclosing_for;

		this->stringKeepAlive.push_back(std::move(it_name));
		this->stringKeepAlive.push_back(std::move(lbName));
		this->stringKeepAlive.push_back(std::move(ubName));

		it->innerFor->parallel = CLAST_PARALLEL_NOT; // Disable parallelism for the inner loop

		this->insertedStmts += 1;
		it->outerHeaderStmt = pesto_clast_create_stmt(cloogOptions, nullptr, this->insertedStmts);
		clast_insert_before(&(root->stmt), &(it->outerFor->stmt), &(it->outerHeaderStmt->stmt));

		this->insertedStmts += 1;
		it->outerBodyStmt = pesto_clast_create_stmt(cloogOptions, nullptr, this->insertedStmts);
		clast_insert_indepth(&(it->outerFor->stmt), &(it->outerBodyStmt->stmt));

		this->insertedStmts += 1;
		it->innerBodyStmt0 = pesto_clast_create_stmt(cloogOptions, nullptr, this->insertedStmts);
		clast_insert_indepth(&(it->innerFor->stmt), &(it->innerBodyStmt0->stmt));
	}

	cloog_domain_free(enclosingDomain);
}

void TpzTilingPass::codegen(Bundle &bundle, std::vector<pesto::pass::ptile::ClastTilingPair> &clastTilingPairs,
							isl_set *innerDomain, pesto::maths::Vec2i slope) {

	using pesto::ptile::NameHelper;

	auto &macroData = bundle.metadata.get<pesto::bundle::metadata::Macro>();
	auto &varDeclData = bundle.metadata.get<pesto::bundle::metadata::VarDecl>();

	macroData.nonProcessedList.push_back("#include <omp.h>");

	// TODO: remove (debug)
	isl_set_dump(innerDomain);

	// add var for front iterator
	std::string nthreadsName = std::string("NTHREADS");
	std::string minVolName = std::string("D1_MIN_VOL");
	std::string offsetName = std::string("tile_offset");

	varDeclData.addVar(nthreadsName);
	varDeclData.addVar(offsetName);

	for (size_t i = 0; i < clastTilingPairs.size(); ++i) {
		auto &pair = clastTilingPairs[i];

		NameHelper nameHelper = NameHelper(pair.depth, pair.outerFor->iterator, pair.innerFor->iterator);

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
			char const *dimName = isl_set_get_dim_name(innerDomain, isl_dim_set, i);
			NameHelper helper = NameHelper(pair.depth, pair.outerFor->iterator, dimName);
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

			NameHelper nextHelper =
				NameHelper(1, clastTilingPairs[1].outerFor->iterator, clastTilingPairs[1].innerFor->iterator);

			std::string stmtStr = nameHelper.outerForLBName() + " = 0;";
			stmtStr += nameHelper.outerForUBName() + " = ";
			stmtStr += std::string() + "ceild((" + max_str + ") - (" + min_str + ")";
			stmtStr += ", " + nameHelper.divName() + ") - 1;";
			stmtStr += nthreadsName + " = omp_get_max_threads();";

			macroData.defineFunc("S" + std::to_string(pair.outerHeaderStmt->statement->number), "", stmtStr);

			varDeclData.addVar(nameHelper.outerForUBName());
			varDeclData.addVar(nameHelper.innerForLBName());
			varDeclData.addVar(nameHelper.innerForUBName());
			varDeclData.addVar(nameHelper.pcMaxName());

			stmtStr = nameHelper.innerForLBName() + " = " + pair.outerFor->iterator + " * " + nameHelper.divName() +
					  " + " + min_str + ";";
			stmtStr += nameHelper.innerForUBName() + " = " + "min((" + pair.outerFor->iterator + " + 1) * " +
					   nameHelper.divName() + " - 1 + " + min_str + ", " + max_str + ");";
			macroData.defineFunc("S" + std::to_string(pair.outerBodyStmt->statement->number), "", stmtStr);

			// Trapezoidal bounds
			auto d1Bounds = this->computeInnerMinMaxStr(innerDomain, 1);

			stmtStr = trapezoidalLBName + " = max(max(" + d1Bounds.first + ", " + nextHelper.innerForLBName() +
					  "), max(" + d1Bounds.first + ", " + nextHelper.innerForLBName() + ") + " + offsetName //
					  + " + floord(" + std::to_string(slope.y) + " * (" + pair.innerFor->iterator + " - " +
					  nameHelper.innerForLBName() + "), " + std::to_string(slope.x) + "));";
			stmtStr += trapezoidalUBName + " = min(" + d1Bounds.second + ", " + nextHelper.innerForUBName() + ");";

			macroData.defineFunc(std::string("S") + std::to_string(pair.innerBodyStmt0->statement->number), "",
								 stmtStr);
			stmtStr = "";
			stmtStr += trapezoidalLBName + " = max(" + d1Bounds.first + ", " + nextHelper.innerForLBName() + ");";
			stmtStr += trapezoidalUBName + " = min(min(" + d1Bounds.second + ", " + nextHelper.innerForUBName() +
					   "), max(" + d1Bounds.first + ", " + nextHelper.innerForLBName() + ") + " + offsetName //
					   + " + floord(" + std::to_string(slope.y) + " * (" + pair.innerFor->iterator + " - " +
					   nameHelper.innerForLBName() + "), " + std::to_string(slope.x) + ") - 1);";

			macroData.defineFunc(std::string("S") + std::to_string(pair.innerBodyStmt1->statement->number), "",
								 stmtStr);
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

			if (this->useMinVolume) {
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
			} else {
				stmtStr +=
					nameHelper.targetVolName() + " = " + nameHelper.pcMaxName() + " / " + nameHelper.divName() + ";";
				stmtStr += nameHelper.outerForUBName() + " = max(floord(" + nameHelper.pcMaxName() + ", " +
						   nameHelper.targetVolName() + "), 1) - 1;";
			}

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

			// volume check (fail if the target volume is too small)
			stmtStr += "if(" + nameHelper.innerForUBName() + " - " + nameHelper.innerForLBName() + " + 1 < floord(" +
					   std::to_string(slope.y) + " * " + rootHelper.divName() + ", " + std::to_string(slope.x) +
					   ") && (" + pair.outerFor->iterator + " != " + nameHelper.outerForUBName() + ") && (" +
					   nameHelper.outerForUBName() + " > 0)) {";
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

			macroData.defineFunc(std::string("S") + std::to_string(pair.innerBodyStmt0->statement->number), "", "");
			macroData.defineFunc(std::string("S") + std::to_string(pair.innerBodyStmt1->statement->number), "", "");
		}
		// for depth > 1 we do rectangular tiling
		if (pair.depth > 1) {
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

			macroData.defineFunc(std::string("S") + std::to_string(pair.innerBodyStmt0->statement->number), "", "");
			macroData.defineFunc(std::string("S") + std::to_string(pair.innerBodyStmt1->statement->number), "", "");
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

		isl_set_dump(isl_set_from_cloog_domain(parentFor->domain));

		if (domain == nullptr) {
			domain = isl_union_set_from_set(isl_set_copy(isl_set_from_cloog_domain(parentFor->domain)));
		} else {
			domain = isl_union_set_add_set(domain, isl_set_copy(isl_set_from_cloog_domain(parentFor->domain)));
		}
	}

	free(userStmts);

	isl_set *set = isl_set_from_union_set(domain);
	set = isl_set_coalesce(set);
	set = isl_set_from_basic_set(isl_set_convex_hull(set));

	for (size_t i = 0; i < clastTilingPairs.size(); ++i) {
		ptile::ClastTilingPair &pair = clastTilingPairs[i];
		set = isl_set_set_dim_name(set, isl_dim_set, i, pair.innerFor->iterator);
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

		std::string lbName = nameHelper.innerForLBName();
		std::string ubName = nameHelper.innerForUBName();
		if (pair.depth == 1) {
			lbName = "_tpzlb";
			ubName = "_tpzub";
		}

		for (int i = 0; i < nclastloops; ++i) {
			struct clast_for *clastFor = clastloops[i];

			std::unique_ptr<std::string> lbPtr = std::make_unique<std::string>(lbName);
			std::unique_ptr<std::string> ubPtr = std::make_unique<std::string>(ubName);

			struct clast_name *paramLB = new_clast_name(lbPtr->c_str());
			struct clast_name *paramUB = new_clast_name(ubPtr->c_str());

			struct clast_expr *lbExpr = nullptr;
			struct clast_expr *ubExpr = nullptr;

			if (pair.depth == 1) {
				lbExpr = &(paramLB->expr);
				ubExpr = &(paramUB->expr);
			} else {
				struct clast_reduction *lbRed = new_clast_reduction(clast_red_max, 2);
				lbRed->elts[0] = &(paramLB->expr);
				lbRed->elts[1] = clastFor->LB;

				struct clast_reduction *ubRed = new_clast_reduction(clast_red_min, 2);
				ubRed->elts[0] = &(paramUB->expr);
				ubRed->elts[1] = clastFor->UB;

				lbExpr = &(lbRed->expr);
				ubExpr = &(ubRed->expr);
			}

			clastFor->LB = lbExpr;
			clastFor->UB = ubExpr;

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
	}

	userDirective += ") ";

	std::string privateVars = "";

	// add inner for iterators
	for (auto &pair : clastTilingPairs) {
		if (!privateVars.empty()) {
			privateVars += ", ";
		}
		privateVars += pair.innerFor->iterator;
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

	writes = isl_union_map_apply_domain(writes, isl_union_map_copy(named_schedules));
	reads = isl_union_map_apply_domain(reads, isl_union_map_copy(named_schedules));

	// This is for debugging purposes
	// std::cerr << "Domains: " << std::endl;
	// isl_union_set_dump(domains);
	// std::cerr << "Schedule" << std::endl;
	// isl_union_map_dump(schedules);
	// std::cerr << "Named Schedules" << std::endl;
	// isl_union_map_dump(named_schedules);
	// std::cerr << "Writes: " << std::endl;
	// isl_union_map_dump(writes);
	// std::cerr << "Reads: " << std::endl;
	// isl_union_map_dump(reads);

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
	// dep_flow = isl_union_map_apply_range(dep_flow, isl_union_map_copy(named_schedules));
	// dep_flow = isl_union_map_apply_domain(dep_flow, isl_union_map_copy(named_schedules));

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

	isl_set *set = isl_set_from_union_set(isl_union_set_copy(deltas));

	isl_map *proj = isl_set_identity(isl_set_copy(set));
	if (isl_map_dim(proj, isl_dim_out) > 2) {
		proj = isl_map_remove_dims(proj, isl_dim_out, 2, isl_map_dim(proj, isl_dim_out) - 2);
	}

	set = isl_set_apply(set, proj);
	set = isl_set_drop_constraints_involving_dims(set, isl_dim_param, 0, isl_set_dim(set, isl_dim_param));

	// isl_set_dump(set);

	std::vector<pesto::maths::Vec2i> depDistanceVector;

	// Helper function for isl_set_foreach_basic_set

	isl_set_foreach_basic_set(
		set,
		[](__isl_take isl_basic_set *bset, void *user) -> isl_stat {
			auto *depDistanceVector = static_cast<std::vector<pesto::maths::Vec2i> *>(user);
			isl_point *point = isl_basic_set_sample_point(bset);

			isl_val *x = isl_point_get_coordinate_val(point, isl_dim_set, 0);
			isl_val *y = isl_point_get_coordinate_val(point, isl_dim_set, 1);

			depDistanceVector->push_back(pesto::maths::Vec2i(isl_val_get_num_si(x), isl_val_get_num_si(y)));

			isl_val_free(x);
			isl_val_free(y);
			isl_point_free(point);

			return isl_stat_ok;
		},
		&depDistanceVector);

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

	// DEBUG
	// std::cerr << "Dep vector list: " << std::endl;
	// for (const auto &vec : depDistanceVector) {
	// 	std::cerr << "(" << vec.x << ", " << vec.y << ")" << std::endl;
	// }

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
