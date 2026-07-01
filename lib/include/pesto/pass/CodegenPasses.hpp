/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/pass/Pass.hpp>
#include <pesto/pass/clast/GenClastPass.hpp>

namespace pesto {

namespace pass {

/**
 * A pass that generate C code from a Program
 * For each OslScop it will look for a ClastHolder, and use the clast to
 * generate C code
 * Else it will use cloog on the openscop representation of the OslScop
 */
class CloogCodegenPass : public IPass {
  public:
	CloogCodegenPass();
	virtual ~CloogCodegenPass();

	virtual Bundle apply(Bundle &&bundle) override;
	virtual std::set<std::string> inputIR() const override;
	virtual std::string outputIR() const override;

	void setGenClastPass(std::unique_ptr<pass::GenClastPass> pass);

  private:
	std::unique_ptr<pass::GenClastPass> genClastPass;
};

} // namespace pass

} // namespace pesto
