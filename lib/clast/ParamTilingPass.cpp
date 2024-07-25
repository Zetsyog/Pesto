#include <pesto/ClastPass.hpp>

using namespace pesto;

ClastParamTilingPass::ClastParamTilingPass() {}

ClastParamTilingPass::~ClastParamTilingPass() {}

void ClastParamTilingPass::runSingleNest(ClastNest &nest) {}

std::string ClastParamTilingPass::getUUID() {
	return "clast-param-tiling-pass";
}