#include <pesto/Pesto.hpp>

using namespace pesto;

ClastParamTilingPB::ClastParamTilingPB() {}

ClastParamTilingPB::~ClastParamTilingPB() {}

std::string ClastParamTilingPB::getUUID() {
	return PESTO_UUID_PASS_PARAM_TILING;
}

std::unique_ptr<pesto::IPestoPass> ClastParamTilingPB::build(PMBuilder &builder,
															 const json &j) {
	auto pass = std::make_unique<ClastParamTilingPass>();
	if (j.contains("gen-trahrhe")) {
		pass->gen_trahrhe = j["gen-trahrhe"];
	}
	// TODO: trahrhe parameters
	return pass;
}
