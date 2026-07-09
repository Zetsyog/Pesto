/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/pass/CloogCodegen.hpp>
#include <pesto/passbuilder/CloogCodegenPB.hpp>
#include <pesto/passbuilder/GenClastPB.hpp>

using namespace pesto::builder;

CloogCodegenPB::CloogCodegenPB() {
	logger = log::get_logger("pb/codegen-cloog");
}

CloogCodegenPB::~CloogCodegenPB() {}

std::unique_ptr<pesto::pass::IPass> CloogCodegenPB::build(PassManagerBuilder &builder, const json &j) {
	(void)builder;
	(void)j;

	auto pass = std::make_unique<pass::CloogCodegenPass>();

	auto clastPB = builder.getPassBuilder(GenClastPB::getID());
	if (!clastPB.has_value()) {
		logger->error(
			"GenClastPB pass builder not found. CloogCodegenPass requires GenClastPass to be generated. Skipping.");
		throw std::runtime_error("GenClastPB pass builder not found");
	}
	std::unique_ptr<pass::GenClastPass> clastPass = std::unique_ptr<pass::GenClastPass>(
		static_cast<pass::GenClastPass *>(clastPB->get().build(builder, j).release()));

	if (clastPass) {
		pass->setGenClastPass(std::move(clastPass));
	} else {
		logger->error("Could not create clast pass");
	}

	return pass;
}
