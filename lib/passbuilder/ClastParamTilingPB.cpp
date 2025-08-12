#include <pesto/pass/clast/ParamTilingPass.hpp>
#include <pesto/passbuilder/ClastParamTilingPB.hpp>

using namespace pesto::builder;

ClastParamTilingPB::ClastParamTilingPB() {}

ClastParamTilingPB::~ClastParamTilingPB() {}

std::unique_ptr<pesto::pass::IPass> ClastParamTilingPB::build(PassManagerBuilder &builder, const json &j) {
	using pesto::pass::ClastParamTilingPass;

	(void)builder;

	std::unique_ptr<ClastParamTilingPass> pass = std::make_unique<ClastParamTilingPass>();

	// TODO: cleaner parsing
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
				pass->setBoundComputeMethod(ptile::codegen::BoundComputeMethod::FILL_LAST);
			} else if (val == "fill-first") {
				pass->setBoundComputeMethod(ptile::codegen::BoundComputeMethod::FILL_FIRST);
			} else {
				std::cerr << "Warning: unknown bound compute method \"" << val << "\". Default to fill-last."
						  << std::endl;
				pass->setBoundComputeMethod(ptile::codegen::BoundComputeMethod::FILL_LAST);
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
			pass->setTileSizeHint(val);
			continue;
		}
		if (key == "boundCorrectionStrategy") {
			if (val == "no-correction") {
				pass->setBoundCorrectionStrategy(ptile::codegen::BoundCorrectionStrategy::NO_CORRECTION);
			} else if (val == "correct") {
				pass->setBoundCorrectionStrategy(ptile::codegen::BoundCorrectionStrategy::CORRECT);
			} else if (val == "shout-and-continue") {
				pass->setBoundCorrectionStrategy(ptile::codegen::BoundCorrectionStrategy::SHOUT_AND_CONTINUE);
			} else if (val == "shout-and-die") {
				pass->setBoundCorrectionStrategy(ptile::codegen::BoundCorrectionStrategy::SHOUT_AND_DIE);
			} else {
				std::cerr << "Warning: unknown bound correction strategy \"" << val << "\". Skipping." << std::endl;
			}
			continue;
		}
		if (key == "volumeCheck") {
			if (!val.is_object()) {
				std::cerr << "Warning: volumeCheck should be an object. Skipping." << std::endl;
				continue;
			}
			for (auto &[k, v] : val.items()) {
				if (k == "strategy") {
					if (!v.is_string()) {
						std::cerr << "Warning: volumeCheck strategy should be a string. Skipping." << std::endl;
						continue;
					}
					if (v == "no-check") {
						pass->setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy::NO_CHECK);
					} else if (v == "check-and-die") {
						pass->setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy::CHECK_AND_DIE);
					} else if (v == "check-and-warn") {
						pass->setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy::CHECK_AND_WARN);
					} else if (v == "no-tile-until-threshold") {
						pass->setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy::NO_TILE_UNTIL_THRESHOLD);
					} else if (v == "dynamic-tile-until-threshold") {
						pass->setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy::DYNAMIC_TILE_UNTIL_THRESHOLD);
					} else {
						std::cerr << "Warning: unknown volume check strategy \"" << v << "\". Skipping." << std::endl;
					}
				} else if (k == "trigger") {
					if (!v.is_string()) {
						std::cerr << "Warning: volumeCheck trigger should be a string. Skipping." << std::endl;
						continue;
					}
					if (v == "never") {
						pass->setVolumeCheckTrigger(VolumeCheckTrigger::NEVER);
					} else if (v == "always") {
						pass->setVolumeCheckTrigger(VolumeCheckTrigger::ALWAYS);
					} else if (v == "first-outter-iterator-dependency") {
						pass->setVolumeCheckTrigger(VolumeCheckTrigger::FIRST_OUTTER_ITERATOR_DEPENDENCY);
					} else {
						std::cerr << "Warning: unknown volume check trigger \"" << v << "\". Skipping." << std::endl;
					}
				} else if (k == "threshold") {
					if (!v.is_string()) {
						std::cerr << "Warning: volume check threshold should be a string. Skipping." << std::endl;
						continue;
					}
					pass->setVolumeCheckThreshold(v);
				} else {
					std::cerr << "Warning: unknown volume check option " << k << ". Skipping." << std::endl;
				}
			}
			continue;
		}

		std::cerr << "Warning: unknown option " << key << " in clast-param-tiling pass. Ignoring." << std::endl;
	}

	// TODO: trahrhe parameters
	return pass;
}
