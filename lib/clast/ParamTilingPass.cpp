#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <cloog/isl/cloog.h>

#include <isl/val.h>

#include <trahrhe/trahrhe.h>

#include <pesto/Pesto.hpp>
#include <pesto/clast/clast_support.h>
#include <pesto/isl_support.h>

using pesto::pass::metadata::ClastHolder;
using pesto::pass::metadata::MacroMetadata;
using pesto::pass::metadata::VarDeclMetadata;

pesto::pass::ClastParamTilingPass::ClastParamTilingPass()
	: IPass("pass-param-tiling")
	, gen_trahrhe(false)
	, trahrhe_use_dichotomy(true)
	, is_stencil(false)
	, tiling_types()
	, outputFile(".")
	, ptileUtil() {
	this->gen_clast_pass = std::make_unique<GenClastPass>();
	// TODO: WHY IS THIS HERE?
	this->gen_clast_pass->codegenContext = 1;
}

pesto::pass::ClastParamTilingPass::~ClastParamTilingPass() {}

void pesto::pass::ClastParamTilingPass::applyScop(pesto::OslScop &scop) {
	// generate the clast
	log(std::cerr) << "Generating clast for nest " << scop.getId() << std::endl;
	this->gen_clast_pass->applyScop(scop);
	if (!scop.metadata.has<ClastHolder>()) {
		log(std::cerr) << "Error: "
					   << "Clast generation failed." << std::endl;
		return;
	}
	auto clastData = scop.metadata.get<ClastHolder>().lock();

	ptileUtil.setIdTrahrhePrefix(scop.getId());
	for (auto &type : this->tiling_types) {
		ptileUtil.pushTilingType(type);
	}

	ptileUtil.apply(clastData->cloogProgram, clastData->cloogOptions, (struct clast_root *)clastData->root);
	if (ptileUtil.hasError()) {
		log(std::cerr) << "Error while tiling: " << ptileUtil.getErrorMessage() << std::endl;
	}

	for (auto &name : ptileUtil.getNames()) {
		clastData->names.push_back(name);
	}

	auto macroData = scop.metadata.getOrCreate<MacroMetadata>().lock();
	for (auto &pair : ptileUtil.getStatements()) {
		macroData->list.push_back("#define S" + std::to_string(pair.first) + "() " + pair.second);
	}

	auto varDeclData = scop.metadata.getOrCreate<VarDeclMetadata>().lock();
	for (auto &var : ptileUtil.getVarList()) {
		varDeclData->addVar(var);
	}

	auto trahrheDomains = ptileUtil.getTrahrheDomains();
	for (auto &pair : trahrheDomains) {
		// print trahrhe domain
		log(std::cerr) << "Trahrhe domain = " << pair.second << std::endl;
	}

	if (this->gen_trahrhe) {
		int domainCount = 0;

		std::filesystem::path outputDir = ".";
		std::string filename = "header.";
		if (!this->outputFile.stem().string().empty()) {
			filename = this->outputFile.stem().string() + ".";
		}
		if (this->outputFile.has_parent_path()) {
			outputDir = this->outputFile.parent_path();
		}

		for (auto &pair : trahrheDomains) {
			std::string header_filename =
				filename + "trahrhe.n" + std::to_string(scop.getId()) + ".g" + std::to_string(domainCount) + ".h";

			trahrhe_context_t *trahrhe_ctx = trahrhe_context_alloc();
			trahrhe_context_use_dichotomy(trahrhe_ctx, this->trahrhe_use_dichotomy);
			trahrhe_context_set_verbosity(trahrhe_ctx, TRAHRHE_VERBOSITY_INFO);
			trahrhe_context_parse_input_domain(trahrhe_ctx, (char *)pair.second.c_str());
			trahrhe_context_set_tile_level(trahrhe_ctx, pair.first);

			size_t trahrhe_len = 0;
			char **trahrhe_expr = trahrhe_get_inverted_polynomials_expr(trahrhe_ctx, &trahrhe_len);
			if (trahrhe_len == 0) {
				log(std::cerr) << "Error: "
							   << "Could not compute trahrhe expr" << std::endl;
				continue;
			}
			std::string header_path = (outputDir / header_filename).string();
			log(std::cerr) << "trahrhe header = " << header_path << std::endl;
			FILE *fp = fopen(header_path.c_str(), "w");
			if (fp == NULL) {
				log(std::cerr) << "Error: "
							   << "Could not open file " << header_filename << std::endl;
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
			log(std::cerr) << "Generated trahrhe header file: " << header_filename << std::endl;

			// TODO: function to generate the header file name
			std::string macro = "#include \"" + header_filename + "\"";
			macroData->list.push_back(macro);
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
}

void pesto::pass::ClastParamTilingPass::pushTilingType(ParamTilingType type) { this->tiling_types.push_back(type); }

pesto::ParamTilingType pesto::pass::ClastParamTilingPass::getTilingType(size_t depth) {
	if (this->tiling_types.size() == 0) {
		return ParamTilingType::ALGEBRAIC;
	}
	if (depth < this->tiling_types.size()) {
		return this->tiling_types[depth];
	}
	return this->tiling_types[this->tiling_types.size() - 1];
}

void pesto::pass::ClastParamTilingPass::hookBegin(pesto::Program &prog) {
	if (!prog.getOutputFile().string().empty()) {
		this->outputFile = prog.getOutputFile();
	}
}

void pesto::pass::ClastParamTilingPass::setTileSizeHint(int size) { this->ptileUtil.setTileSizeTarget(size); }

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
