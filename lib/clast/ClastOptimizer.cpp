#include <iostream>

#include <pesto/ClastOptimizer.hpp>
#include <pesto/ClastProgram.hpp>
#include <pesto/macros.h>

using namespace pesto;

ClastOptimizer::ClastOptimizer() {}
ClastOptimizer::~ClastOptimizer() {}

void ClastOptimizer::addPass(IClastPass *pass) {
	passes.push_back(std::unique_ptr<IClastPass>(pass));
}

void ClastOptimizer::run(ClastProgram &prog) {
	for (auto &pass : passes) {
		int nestCount = prog.getNests().size();
		std::cerr << "[" << PESTO_NAME_OPT_CLAST << "] Running pass '"
				  << pass->getUUID() << "' on " << std::to_string(nestCount)
				  << " loop nests " << std::endl;
		pass->run(prog);
	}
}
