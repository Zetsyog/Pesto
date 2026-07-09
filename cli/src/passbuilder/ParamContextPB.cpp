/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/pass/ParamContext.hpp>
#include <pesto/passbuilder/ParamContextPB.hpp>

using namespace pesto::builder;

ParamContextPB::ParamContextPB() {
	logger = log::get_logger("pb/param-context");
}

ParamContextPB::~ParamContextPB() {}

std::unique_ptr<pesto::pass::IPass> ParamContextPB::build(PassManagerBuilder &builder, const json &j) {
	(void)builder;

	// TODO Parse the JSON object and build the pass
	if (!j.contains("value")) {
		logger->error("Missing option \"value\" for ParamContextPass");
		throw std::invalid_argument("Missing option \"value\" for ParamContextPass");
	}

	auto pass = std::make_unique<pesto::pass::ParamContextPass>(j["value"]);

	return pass;
}