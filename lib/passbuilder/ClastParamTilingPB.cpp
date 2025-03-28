#include <pesto/Pesto.hpp>

using namespace pesto::builder;

ClastParamTilingPB::ClastParamTilingPB() {}

ClastParamTilingPB::~ClastParamTilingPB() {}

std::unique_ptr<pesto::pass::IPass>
ClastParamTilingPB::build(PassManagerBuilder &builder, const json &j) {
	using pesto::pass::ClastParamTilingPass;

	(void)builder;

	std::unique_ptr<ClastParamTilingPass> pass =
		std::make_unique<ClastParamTilingPass>();
	if (j.contains("gen-trahrhe")) {
		pass->gen_trahrhe = j["gen-trahrhe"];
	}
	if (j.contains("trahrhe-mask")) {
		std::cerr
			<< "Warning: trahrhe-mask is deprecated. Use trahrhe-use-dichtomy"
			   "attribute instead."
			<< std::endl;
		pass->trahrhe_mask = j["trahrhe-mask"];
	}
	if (j.contains("trahrhe-use-dichtomy")) {
		pass->trahrhe_use_dichotomy = j["trahrhe-use-dichtomy"];
	}
	if (j.contains("bound-compute-method")) {
		std::string method = j["bound-compute-method"];
		if (method == "fill-last" || method == "default") {
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
		std::cerr
			<< "Warning: pluto pass is deprecated. You should call a pluto "
			   "pass before the param-tiling pass instead."
			<< std::endl;
	}

	if (j.contains("tile-size-hint")) {
		pass->tileSizeHint = j["tile-size-hint"];
	}

	// TODO: trahrhe parameters
	return pass;
}
