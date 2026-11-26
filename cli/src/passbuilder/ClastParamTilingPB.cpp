/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/pass/clast/ParamTiling.hpp>
#include <pesto/passbuilder/ClastParamTilingPB.hpp>
#include <pesto/passbuilder/GenClastPB.hpp>

using namespace pesto::builder;

ClastParamTilingPB::ClastParamTilingPB() {
	logger = log::get_logger("pb/param-tiling");
}

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
			logger->warn("Trahrhe-mask is deprecated. Use "
						 "trahrhe-use-dichotomy"
						 "attribute instead.");
			continue;
		}
		if (key == "trahrhe-use-dichotomy") {
			pass->trahrhe_use_dichotomy = val;
			continue;
		}
		if (key == "bound-compute-method") {
			if (!val.is_string()) {
				logger->error("Bound compute method must be a string.");
				throw std::invalid_argument("Bound compute method must be a string");
			}
			std::string valStr = val.get<std::string>();
			if (valStr == "fill-last" || valStr == "default") {
				pass->setBoundComputeMethod(ptile::codegen::BoundComputeMethod::FILL_LAST);
			} else if (valStr == "fill-first") {
				pass->setBoundComputeMethod(ptile::codegen::BoundComputeMethod::FILL_FIRST);
			} else {
				logger->warn("Unknown bound compute method \"{}\". Default to fill-last.", valStr.c_str());
				pass->setBoundComputeMethod(ptile::codegen::BoundComputeMethod::FILL_LAST);
			}
			continue;
		}
		if (key == "types") {
			if (!val.is_array()) {
				logger->warn("Types must be an array. Skipping.");
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
				} else if (elt == "last-dim-rect") {
					type = ParamTilingType::LAST_DIM_RECT;
				} else {
					type = ParamTilingType::ALGEBRAIC;
					logger->warn("Unknown param tiling type. Default to "
								 "algebraic.");
				}
				pass->pushTilingType(type);
			}
			continue;
		}
		if (key == "pluto") {
			logger->warn("Pluto pass is deprecated. You should call a pluto "
						 "pass before the param-tiling pass instead.");
			continue;
		}
		if (key == "boundCorrectionStrategy") {
			if (!val.is_string()) {
				logger->error("Bound correction strategy must be a string. Skipping.");
				throw std::invalid_argument("Bound correction strategy must be a string");
			}
			std::string valStr = val.get<std::string>();
			if (valStr == "no-correction") {
				pass->setBoundCorrectionStrategy(ptile::codegen::BoundCorrectionStrategy::NO_CORRECTION);
			} else if (valStr == "correct") {
				pass->setBoundCorrectionStrategy(ptile::codegen::BoundCorrectionStrategy::CORRECT);
			} else if (valStr == "shout-and-continue") {
				pass->setBoundCorrectionStrategy(ptile::codegen::BoundCorrectionStrategy::SHOUT_AND_CONTINUE);
			} else if (valStr == "shout-and-die") {
				pass->setBoundCorrectionStrategy(ptile::codegen::BoundCorrectionStrategy::SHOUT_AND_DIE);
			} else {
				logger->warn("unknown bound correction strategy \"{}\". Skipping.", valStr.c_str());
			}
			continue;
		}
		if (key == "volumeCheck") {
			if (!val.is_object()) {
				logger->error("volumeCheck must be an object. Skipping.");
				throw std::invalid_argument("volumeCheck must be an object");
			}
			for (auto &[k, v] : val.items()) {
				if (k == "strategy") {
					if (!v.is_string()) {
						logger->error("volumeCheck strategy must be a string.");
						throw std::invalid_argument("volumeCheck strategy must be a string");
					}
					std::string vStr = v.get<std::string>();
					if (vStr == "no-check") {
						pass->setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy::NO_CHECK);
					} else if (vStr == "check-and-die") {
						pass->setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy::CHECK_AND_DIE);
					} else if (vStr == "check-and-warn") {
						pass->setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy::CHECK_AND_WARN);
					} else if (vStr == "no-tile-until-threshold") {
						pass->setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy::NO_TILE_UNTIL_THRESHOLD);
					} else if (vStr == "dynamic-tile-until-threshold") {
						pass->setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy::DYNAMIC_TILE_UNTIL_THRESHOLD);
					} else {
						logger->warn("Unknown volume check strategy \"{}\". Skipping.", vStr.c_str());
					}
				} else if (k == "trigger") {
					if (!v.is_string()) {
						logger->error("volumeCheck trigger must be a string. Skipping.");
						throw std::invalid_argument("volumeCheck trigger must be a string");
					}
					std::string vStr = v.get<std::string>();
					if (vStr == "never") {
						pass->setVolumeCheckTrigger(VolumeCheckTrigger::NEVER);
					} else if (vStr == "always") {
						pass->setVolumeCheckTrigger(VolumeCheckTrigger::ALWAYS);
					} else if (vStr == "first-outer-iterator-dependency") {
						pass->setVolumeCheckTrigger(VolumeCheckTrigger::FIRST_OUTER_ITERATOR_DEPENDENCY);
					} else {
						logger->warn("Unknown volume check trigger \"{}\". Skipping.", vStr.c_str());
					}
				} else if (k == "threshold") {
					if (!v.is_string()) {
						logger->error("volumeCheck threshold must be a string. Skipping.");
						throw std::invalid_argument("volumeCheck threshold must be a string");
					}
					std::string vStr = v.get<std::string>();
					pass->setVolumeCheckThreshold(vStr);
				} else {
					logger->warn("Unknown volume check option {}. Skipping.", k.c_str());
				}
			}
			continue;
		}
		if (key == "clast") {
			if (!val.is_object()) {
				logger->error("'clast' attribute must be an object.");
				throw std::invalid_argument("'clast' attribute must be an object");
			}
			auto clastPB = builder.getPassBuilder(GenClastPB::getID());
			if (!clastPB.has_value()) {
				logger->error("GenClastPB pass builder not found. 'clast' attribute require GenClastPass to be "
							  "generated. Make sure to register the correct builder ");
				throw std::runtime_error("'" + GenClastPB::getID() + "' pass builder not found");
			}
			// Apply on the preset pass so defaults not given in 'clast' are kept.
			static_cast<GenClastPB &>(clastPB->get()).configure(pass->getGenClastPass(), val);
			continue;
		}

		logger->warn("Unknown option {} in clast-param-tiling pass. Ignoring.", key.c_str());
	}

	// TODO: trahrhe parameters
	return pass;
}
