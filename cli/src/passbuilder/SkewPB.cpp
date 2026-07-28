/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/pass/SkewPass.hpp>
#include <pesto/passbuilder/SkewPB.hpp>

using namespace pesto::builder;

SkewPB::SkewPB() {
	logger = log::get_logger("pb/skew");
}

SkewPB::~SkewPB() {}

std::unique_ptr<pesto::pass::IPass> SkewPB::build(PassManagerBuilder &builder, const json &j) {
	using pesto::pass::SkewPass;

	(void)builder;

	std::unique_ptr<SkewPass> pass = std::make_unique<SkewPass>();

	// transform is optional: if present (DP kernels like knuth), apply it; if
	// absent (stencils like seidel), SkewPass::apply detects the skewing that
	// Pluto already applied, from the current scattering.
	if (j.contains("transform")) {
		std::vector<std::string> lines;
		for (const auto &line : j["transform"])
			lines.push_back(line.get<std::string>());
		pass->setTransform(lines);
	}
	return pass;
}

