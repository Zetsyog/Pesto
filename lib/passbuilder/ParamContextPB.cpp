/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/pass/OslPass.hpp>
#include <pesto/passbuilder/ParamContextPB.hpp>

using namespace pesto::builder;

ParamContextPB::ParamContextPB() {}

ParamContextPB::~ParamContextPB() {}

std::unique_ptr<pesto::pass::IPass> ParamContextPB::build(PassManagerBuilder &builder, const json &j) {
	(void)builder;

	// TODO Parse the JSON object and build the pass
	if (!j.contains("value")) {
		std::cerr << "[pass-builder] Missing option \".value\" for ParamContextPass" << std::endl;
		return std::unique_ptr<pesto::pass::ParamContextPass>{};
	}

	auto pass = std::make_unique<pesto::pass::ParamContextPass>(j["value"]);

	return pass;
}