#include <iostream>

#include <pesto/ClastPassManager.hpp>
#include <pesto/ClastProgram.hpp>
#include <pesto/macros.h>

using namespace pesto;

ClastPassManager::ClastPassManager() {}
ClastPassManager::~ClastPassManager() {}

void ClastPassManager::addPass(std::unique_ptr<IClastPass> pass) {
	passes.push_back(std::move(pass));
}

void ClastPassManager::run(ClastProgram &prog) {
	for (auto &pass : passes) {
		int nestCount = prog.getNests().size();
		std::cerr << "[" << PESTO_NAME_OPT_CLAST << "] Running pass '"
				  << pass->getUUID() << "' on " << std::to_string(nestCount)
				  << " loop nests " << std::endl;
		pass->run(prog);
	}
}
