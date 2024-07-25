#include <pesto/ClastOptimizer.hpp>
#include <pesto/ClastProgram.hpp>

using namespace pesto;

ClastOptimizer::ClastOptimizer() {}
ClastOptimizer::~ClastOptimizer() {}

void ClastOptimizer::addPass(IClastPass *pass) {
	passes.push_back(std::unique_ptr<IClastPass>(pass));
}

void ClastOptimizer::run(ClastProgram &prog) {
	for (auto &pass : passes) {
		pass->run(prog);
	}
}
