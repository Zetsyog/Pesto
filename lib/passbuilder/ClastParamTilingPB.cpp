#include <pesto/Pesto.hpp>

using namespace pesto::builder;

ClastParamTilingPB::ClastParamTilingPB() {}

ClastParamTilingPB::~ClastParamTilingPB() {}

std::unique_ptr<pesto::pass::IPass> ClastParamTilingPB::build(PassManagerBuilder &builder, const json &j) {
	using pesto::pass::ClastParamTilingPass;

	(void)builder;

	std::unique_ptr<ClastParamTilingPass> pass = std::make_unique<ClastParamTilingPass>();

	for (auto &[key, val] : j.items()) {
		if (key == "uuid") {
			continue;
		}
		if (key == "gen-trahrhe") {
			pass->gen_trahrhe = val;
			continue;
		}
		if (key == "trahrhe-mask") {
			std::cerr << "Warning: trahrhe-mask is deprecated. Use "
						 "trahrhe-use-dichtomy"
						 "attribute instead."
					  << std::endl;
			continue;
		}
		if (key == "trahrhe-use-dichotomy") {
			pass->trahrhe_use_dichotomy = val;
			continue;
		}
		if (key == "bound-compute-method") {
			if (val == "fill-last" || val == "default") {
				pass->boundComputeMethod = trahrhe_compute_fill_last;
			} else if (val == "reduce-control") {
				pass->boundComputeMethod = trahrhe_compute_reduce_control;
			} else {
				std::cerr << "Warning: unknown bound compute method \"" << val << "\". Default to fill-last."
						  << std::endl;
				pass->boundComputeMethod = trahrhe_compute_fill_last;
			}
			continue;
		}
		if (key == "types") {
			if (!val.is_array()) {
				std::cerr << "Warning: types should be an array. Skipping." << std::endl;
				continue;
			}
			for (std::string elt : val) {
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
			continue;
		}
		if (key == "pluto") {
			std::cerr << "Warning: pluto pass is deprecated. You should call a pluto "
						 "pass before the param-tiling pass instead."
					  << std::endl;
			continue;
		}
		if (key == "tile-size-hint") {
			if (!val.is_number_integer()) {
				std::cerr << "Warning: tile-size-hint should be an integer. "
							 "Skipping."
						  << std::endl;
				continue;
			}
			pass->tileSizeHint = j["tile-size-hint"];
			continue;
		}

		std::cerr << "Warning: unknown option \"" << key << "\" in clast-param-tiling pass. Ignoring." << std::endl;
	}

	// TODO: trahrhe parameters
	return pass;
}
