#include <pesto/Pesto.hpp>

using namespace pesto;

ClastParamTilingPB::ClastParamTilingPB() {}

ClastParamTilingPB::~ClastParamTilingPB() {}

std::unique_ptr<pesto::IPestoPass> ClastParamTilingPB::build(PMBuilder &builder,
															 const json &j) {
	std::unique_ptr<ClastParamTilingPass> pass =
		std::make_unique<ClastParamTilingPass>();
	if (j.contains("gen-trahrhe")) {
		pass->gen_trahrhe = j["gen-trahrhe"];
	}

	if (j.contains("types")) {
		for (std::string elt : j["types"]) {
			ParamTilingType type;
			if (elt == "algebraic") {
				type = ParamTilingType::ALGEBRAIC;
			} else if (elt == "rectangular") {
				type = ParamTilingType::RECTANGULAR;
			} else if (elt == "scheduled") {
				type = ParamTilingType::SCHEDULED;
			} else {
				type = ParamTilingType::ALGEBRAIC;
				std::cerr << "Warning: unknown param tiling type. Default to "
							 "algebraic."
						  << std::endl;
			}
			pass->pushTilingType(type);
		}
	}

	if (j.contains("pluto")) {
		PlutoPB pluto_pb = PlutoPB();
		std::unique_ptr<IPestoPass> plutoPass =
			pluto_pb.build(builder, j["pluto"]);
		pass->setPlutoPass(std::move(plutoPass));
	}

	// TODO: trahrhe parameters
	return pass;
}
