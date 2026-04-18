/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <cloog/isl/cloog.h>

#include <osl/osl.h>

#include <isl/val.h>

#include <trahrhe/trahrhe.h>

#include <pesto/ParamTilingUtility.hpp>
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

pesto::pass::ClastParamTilingPass::ClastParamTilingPass()
	: IPass("param-tiling"), gen_trahrhe(false), trahrhe_use_dichotomy(true), tiling_types(), ptileUtil() {
	this->gen_clast_pass = std::make_unique<GenClastPass>();
	this->gen_clast_pass->cloogF = -1;
	this->gen_clast_pass->cloogL = 1;
	// TODO: WHY IS THIS HERE?
	// This should be set in a separate pass
	this->gen_clast_pass->codegenContext = 1;
}

pesto::pass::ClastParamTilingPass::~ClastParamTilingPass() {}

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

		ptileUtil.clearTilingTypes();
		ptileUtil.pushTilingType(ParamTilingType::SCHEDULED);
		ptileUtil.pushTilingType(ParamTilingType::SCHEDULED);
		ptileUtil.pushTilingType(ParamTilingType::RECTANGULAR);

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

	ptileUtil.setIdTrahrhePrefix(0);
	for (auto &type : this->tiling_types) {
		ptileUtil.pushTilingType(type);
	}

	CloogProgram *program = clastIR->getCloogProgram();
	CloogOptions *options = clastIR->getCloogOptions();
	struct clast_root *root = (struct clast_root *)clastIR->get();

	ptileUtil.apply(program, options, root);
	if (ptileUtil.hasError()) {
		std::string errorMessage = ptileUtil.getErrorMessage();
		logger->error("Error while tiling: {}", errorMessage.c_str());
	}

	for (auto &name : ptileUtil.getNames()) {
		clastIR->names.push_back(name);
	}

	auto &macroData = bundle.metadata.get<pesto::bundle::metadata::Macro>();
	for (auto &pair : ptileUtil.getStatements()) {
		macroData.defineFunc("S" + std::to_string(pair.first), "", pair.second);
	}

	auto &varDeclData = bundle.metadata.get<pesto::bundle::metadata::VarDecl>();
	for (auto &var : ptileUtil.getVarList()) {
		varDeclData.addVar(var);
	}

	auto trahrheDomains = ptileUtil.getTrahrheDomains();
	int maxTrahrheTileLevel = 0;
	for (auto &pair : trahrheDomains) {
		// print trahrhe domain
		logger->debug("Trahrhe domain = {}", pair.second.c_str());
		if (pair.first > maxTrahrheTileLevel) {
			maxTrahrheTileLevel = pair.first;
		}
	}
	for (int i = 0; i <= maxTrahrheTileLevel; i++) {
		macroData.list.push_back("#ifndef DIV" + std::to_string(i) +
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

		for (auto &pair : trahrheDomains) {
			std::string header_filename =
				filename + "trahrhe.n" + std::to_string(0) + ".g" + std::to_string(domainCount) + ".h";

			trahrhe_context_t *trahrhe_ctx = trahrhe_context_alloc();
			trahrhe_context_use_dichotomy(trahrhe_ctx, this->trahrhe_use_dichotomy);
			trahrhe_context_set_verbosity(trahrhe_ctx, TRAHRHE_VERBOSITY_INFO);
			trahrhe_context_parse_input_domain(trahrhe_ctx, (char *)pair.second.c_str());
			trahrhe_context_set_tile_level(trahrhe_ctx, pair.first);

			size_t trahrhe_len = 0;
			char **trahrhe_expr = trahrhe_get_inverted_polynomials_expr(trahrhe_ctx, &trahrhe_len);
			if (trahrhe_len == 0) {
				logger->error("Could not compute trahrhe expr");
				continue;
			}
			std::string header_path = (outputDir / header_filename).string();
			logger->debug("trahrhe header = {}", header_path.c_str());
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
	if (ptileUtil.getGeneratedScheduleHeaderFile().has_value()) {
		auto scheduleHeaderFile = ptileUtil.getGeneratedScheduleHeaderFile().value();

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

void pesto::pass::ClastParamTilingPass::pushTilingType(ParamTilingType type) {
	this->tiling_types.push_back(type);
}

pesto::ParamTilingType pesto::pass::ClastParamTilingPass::getTilingType(size_t depth) {
	if (this->tiling_types.size() == 0) {
		return ParamTilingType::ALGEBRAIC;
	}
	if (depth < this->tiling_types.size()) {
		return this->tiling_types[depth];
	}
	return this->tiling_types[this->tiling_types.size() - 1];
}

void pesto::pass::ClastParamTilingPass::setTileSizeHint(int size) {
	this->ptileUtil.setTileSizeTarget(size);
}

void pesto::pass::ClastParamTilingPass::setBoundComputeMethod(ptile::codegen::BoundComputeMethod method) {
	this->ptileUtil.setBoundComputingMethod(method);
}
void pesto::pass::ClastParamTilingPass::setBoundCorrectionStrategy(ptile::codegen::BoundCorrectionStrategy strategy) {
	this->ptileUtil.setBoundCorrectionStrategy(strategy);
}
void pesto::pass::ClastParamTilingPass::setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy strategy) {
	this->ptileUtil.setVolumeCheckStrategy(strategy);
}

void pesto::pass::ClastParamTilingPass::setVolumeCheckTrigger(VolumeCheckTrigger trigger) {
	this->ptileUtil.setVolumeCheckTrigger(trigger);
}

void pesto::pass::ClastParamTilingPass::setVolumeCheckThreshold(std::string threshold) {
	this->ptileUtil.setVolumeCheckThreshold(threshold);
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