/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/pass/clast/CustomVecPragma.hpp>
#include <pesto/passbuilder/ClastCustomVecPragmaPB.hpp>

using namespace pesto::builder;

ClastCustomVecPragmaPB::ClastCustomVecPragmaPB() {}

ClastCustomVecPragmaPB::~ClastCustomVecPragmaPB() {}

std::unique_ptr<pesto::pass::IPass> ClastCustomVecPragmaPB::build(PassManagerBuilder &builder, const json &j) {
	using pesto::pass::CustomVecPragma;

	(void)builder;

	std::unique_ptr<CustomVecPragma> pass = std::make_unique<CustomVecPragma>();

	if (!j.contains("pragma")) {
		std::cerr << "Warning: missing pragma definition in custom vec pragma pass. "
					 "Pass won't be registered."
				  << std::endl;
		return nullptr;
	}
	pass->setPragma(j["pragma"]);

	return pass;
}
