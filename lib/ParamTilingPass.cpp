#include <pesto/ClastPass.hpp>

using namespace pesto;

ClastParamTilingPass::ClastParamTilingPass() {}

ClastParamTilingPass::~ClastParamTilingPass() {}

void ClastParamTilingPass::run(ClastProgram &prog) {}

std::string ClastParamTilingPass::getUUID() {
	return "clast-param-tiling-pass";
}