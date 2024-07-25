#include <pesto/ClastPass.hpp>

using namespace pesto;

IClastPass::~IClastPass() {}

IClastSingleNestPass::~IClastSingleNestPass() {}

void IClastSingleNestPass::run(ClastProgram &prog) {
	for (auto &nest : prog.getNests()) {
		runSingleNest(*nest);
	}
}