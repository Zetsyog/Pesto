/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/pass/TpzTiling.hpp>
#include <pesto/passbuilder/GenClastPB.hpp>
#include <pesto/passbuilder/TpzTilingPB.hpp>

using namespace pesto::builder;

TpzTilingPB::TpzTilingPB() {
	logger = log::get_logger("pb/tpz-tiling");
}

TpzTilingPB::~TpzTilingPB() {}

std::unique_ptr<pesto::pass::IPass> TpzTilingPB::build(PassManagerBuilder &builder, const json &j) {
	using pesto::pass::TpzTilingPass;

	std::unique_ptr<TpzTilingPass> pass = std::make_unique<TpzTilingPass>();

	for (auto &[key, val] : j.items()) {
		// ignore uuid
		if (key == "uuid") {
			continue;
		} else if (key == "useMinVolume") {
			if (!val.is_boolean()) {
				logger->warn("Option {} must be a boolean. Ignoring.", key.c_str());
				continue;
			}
			pass->setUseMinVolume(val.get<bool>());
		} else if (key == "reductionTileAboveGap") {
			if (!val.is_boolean()) {
				logger->warn("Option {} must be a boolean. Ignoring.", key.c_str());
				continue;
			}
			pass->setReductionAboveGap(val.get<bool>());
		} else if (key == "tileReduction") {
			if (!val.is_boolean()) {
				logger->warn("Option {} must be a boolean. Ignoring.", key.c_str());
				continue;
			}
			pass->setTileReduction(val.get<bool>());
		} else if (key == "clast") {
			if (!val.is_object()) {
				logger->error("'clast' attribute must be an object.");
				throw std::invalid_argument("'clast' attribute must be an object");
			}
			auto clastPB = builder.getPassBuilder(GenClastPB::getID());
			if (!clastPB.has_value()) {
				logger->error("'{}' pass builder not found. 'clast' attribute requires it.", GenClastPB::getID());
				throw std::runtime_error("'" + GenClastPB::getID() + "' pass builder not found");
			}
			// Apply on the preset pass so defaults not given in 'clast' are kept.
			static_cast<GenClastPB &>(clastPB->get()).configure(pass->getGenClastPass(), val);
		} else {
			logger->warn("Unknown option {}. Ignoring.", key.c_str());
		}
	}

	return pass;
}
