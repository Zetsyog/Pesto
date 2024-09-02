#include <pesto/Pesto.hpp>

using namespace pesto;

ClastParamTilingPB::ClastParamTilingPB() {}

ClastParamTilingPB::~ClastParamTilingPB() {}

std::string ClastParamTilingPB::getUUID() {
	return PESTO_UUID_PASS_PARAM_TILING;
}

std::unique_ptr<pesto::IPolyhedralPass>
ClastParamTilingPB::build(const json &j) {
	return std::make_unique<ClastParamTilingPass>();
}
