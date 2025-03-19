#include <pesto/Pesto.hpp>

using namespace pesto::builder;

ClastParamTilingPB::ClastParamTilingPB() {}

ClastParamTilingPB::~ClastParamTilingPB() {}

std::unique_ptr<pesto::pass::IPass>
ClastParamTilingPB::build(PassManagerBuilder &builder, const json &j) {
	using pesto::pass::ClastParamTilingPass;

	std::unique_ptr<ClastParamTilingPass> pass =
		std::make_unique<ClastParamTilingPass>();
	if (j.contains("gen-trahrhe")) {
		pass->gen_trahrhe = j["gen-trahrhe"];
	}
	if (j.contains("trahrhe-mask")) {
		pass->trahrhe_mask = j["trahrhe-mask"];
	}
	if (j.contains("bound-compute-method")) {
		std::string method = j["bound-compute-method"];
		if (method == "fill-last") {
			pass->boundComputeMethod = trahrhe_compute_fill_last;
		} else if (method == "reduce-control") {
			pass->boundComputeMethod = trahrhe_compute_reduce_control;
		} else {
			std::cerr << "Warning: unknown bound compute method \"" << method
					  << "\". Default to fill-last." << std::endl;
			pass->boundComputeMethod = trahrhe_compute_fill_last;
		}
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
		std::unique_ptr<pesto::pass::IPass> plutoPass =
			pluto_pb.build(builder, j["pluto"]);
		pass->setPlutoPass(std::move(plutoPass));
	}

	// TODO: trahrhe parameters
	return pass;
}
