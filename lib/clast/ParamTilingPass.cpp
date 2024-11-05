#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <cloog/isl/cloog.h>

#include <isl/val.h>

#include <pesto/Pesto.hpp>

#include "clast/clast_support.h"

#include "isl_support.h"

using pesto::pass::metadata::ClastHolder;
using pesto::pass::metadata::MacroMetadata;
using pesto::pass::metadata::VarDeclMetadata;

pesto::pass::ClastParamTilingPass::ClastParamTilingPass()
	: IPass("pass-param-tiling"), gen_trahrhe(false), is_stencil(false),
	  tiling_types() {
	using namespace pesto::pass;

	this->pluto_pass = std::make_unique<PlutoPass>();
	this->gen_clast_pass = std::make_unique<GenClastPass>();
	this->gen_clast_pass->codegenContext = 10;
}

pesto::pass::ClastParamTilingPass::~ClastParamTilingPass() {}

void pesto::pass::ClastParamTilingPass::applyScop(pesto::OslScop &scop) {
	// run the pass on each nest
	auto tilingUtil = std::make_unique<ParamTilingUtility>();

	// First we apply pluto
	log(std::cerr) << "Running pluto pass on nest " << scop.getId()
				   << std::endl;
	((PlutoPass *)this->pluto_pass.get())->applyScop(scop);

	// Then we generate the clast
	log(std::cerr) << "Generating clast for nest " << scop.getId() << std::endl;
	this->gen_clast_pass->applyScop(scop);
	auto clastData = scop.metadata.get<ClastHolder>();
	if (clastData.expired()) {
		log(std::cerr) << "Error: "
					   << "No clast data found" << std::endl;
	}
	tilingUtil->setScop(scop.getScop());
	tilingUtil->setIdTrahrhePrefix(scop.getId());

	std::shared_ptr<ClastHolder> clastDataShared = clastData.lock();
	tilingUtil->setClastRoot((struct clast_root *)clastDataShared->root);
	tilingUtil->setCloogProgram(clastDataShared->cloogProgram);
	tilingUtil->setCloogOptions(clastDataShared->cloogOptions);

	tilingUtil->apply();
	if (tilingUtil->hasError()) {
		log(std::cerr) << "Error while tiling: "
					   << tilingUtil->getErrorMessage() << std::endl;
	}

	for (auto &name : tilingUtil->getNames()) {
		clastDataShared->names.push_back(name);
	}

	auto macroData = scop.metadata.getOrCreate<MacroMetadata>().lock();
	for (auto &pair : tilingUtil->getStatements()) {
		macroData->list.push_back("#define S" + std::to_string(pair.first) +
								  "() " + pair.second);
	}

	auto varDeclData = scop.metadata.getOrCreate<VarDeclMetadata>().lock();
	for (auto &var : tilingUtil->getVarList()) {
		varDeclData->addVar(var);
	}

	auto trahrheDomains = tilingUtil->getTrahrheDomains();
	int domainCount = 0;
	for (auto &pair : trahrheDomains) {
		log(std::cerr) << "Trahrhe domain = " << pair.second << std::endl;
		if (this->gen_trahrhe) {
			std::ofstream outfile;
			outfile.open(".exec-once", std::ios::app);
			outfile << "trahrhe_wrapper trahrhe_header.n" << scop.getId()
					<< ".g" << domainCount << ".h ";
			outfile << " -d\"" << pair.second << "\"";
			outfile << " -e -t" << pair.first << " -m\"" << this->trahrhe_mask
					<< "\" \n";
			outfile.close();
		}
		// TODO: function to generate the header file name
		std::string macro = "#include \"trahrhe_header.n" +
							std::to_string(scop.getId()) + ".g" +
							std::to_string(domainCount) + ".h\"";
		macroData->list.push_back(macro);
		domainCount++;
	}
}

void pesto::pass::ClastParamTilingPass::pushTilingType(ParamTilingType type) {
	this->tiling_types.push_back(type);
}

pesto::pass::ParamTilingType
pesto::pass::ClastParamTilingPass::getTilingType(int depth) {
	if (this->tiling_types.size() == 0) {
		return ParamTilingType::ALGEBRAIC;
	}
	if (depth < this->tiling_types.size()) {
		return this->tiling_types[depth];
	}
	return this->tiling_types[this->tiling_types.size() - 1];
}

void pesto::pass::ClastParamTilingPass::setPlutoPass(
	std::unique_ptr<pass::IPass> pluto_pass) {
	this->pluto_pass = std::move(pluto_pass);
}
