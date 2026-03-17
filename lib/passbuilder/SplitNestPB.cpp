/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/pass/OslPass.hpp>
#include <pesto/passbuilder/SplitNestPB.hpp>

using namespace pesto::builder;

SplitNestPB::SplitNestPB() {}

SplitNestPB::~SplitNestPB() {}

std::unique_ptr<pesto::pass::IPass> SplitNestPB::build(PassManagerBuilder &builder, const json &j) {
	(void)builder;
	(void)j;
	return std::make_unique<pesto::pass::SplitNestedLoopsPass>();
}