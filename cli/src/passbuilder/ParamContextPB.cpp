/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/pass/ParamContext.hpp>
#include <pesto/passbuilder/ParamConstraintJson.hpp>
#include <pesto/passbuilder/ParamContextPB.hpp>

using namespace pesto::builder;

ParamContextPB::ParamContextPB() {
	logger = log::get_logger("pb/param-context");
}

ParamContextPB::~ParamContextPB() {}

std::unique_ptr<pesto::pass::IPass> ParamContextPB::build(PassManagerBuilder &builder, const json &j) {
	(void)builder;

	if (!j.contains("value")) {
		logger->error("Missing option \"value\" for ParamContextPass");
		throw std::invalid_argument("Missing option \"value\" for ParamContextPass");
	}

	for (auto &[key, val] : j.items()) {
		if (key != "uuid" && key != "value") {
			logger->warn("Unknown option {} in param-context pass. Ignoring.", key.c_str());
		}
	}

	auto pass = std::make_unique<pesto::pass::ParamContextPass>(parseParamConstraint(j["value"], "value", *logger));

	return pass;
}