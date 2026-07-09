/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/pass/TpzTiling.hpp>
#include <pesto/passbuilder/TpzTilingPB.hpp>

using namespace pesto::builder;

TpzTilingPB::TpzTilingPB() {
	logger = log::get_logger("pb/tpz-tiling");
}

TpzTilingPB::~TpzTilingPB() {}

std::unique_ptr<pesto::pass::IPass> TpzTilingPB::build(PassManagerBuilder &builder, const json &j) {
	using pesto::pass::TpzTilingPass;

	(void)builder;

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

		} else {
			logger->warn("Unknown option {}. Ignoring.", key.c_str());
		}
	}

	return pass;
}
