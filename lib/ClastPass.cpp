#include <pesto/ClastPass.hpp>

using namespace pesto;

IClastPass::~IClastPass() {}

IClastSingleNestPass::~IClastSingleNestPass() {}

void IClastSingleNestPass::run(ClastProgram &prog) {
	for (auto &nest : prog.getNests()) {
		runSingleNest(*nest);
	}
}

ClastParamTilingPass::ClastParamTilingPass() {}

ClastParamTilingPass::~ClastParamTilingPass() {}

std::string ClastParamTilingPass::getUUID() { return "clast-param-tiling"; }

void ClastParamTilingPass::run(ClastProgram &prog) {
	for (auto &nest : prog.getNests()) {
		runSingleNest(*nest);
	}
}
