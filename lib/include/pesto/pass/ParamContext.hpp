/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <pesto/ParamConstraint.hpp>
#include <pesto/PassManager.hpp>
#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/pass/Pass.hpp>

namespace pesto {

namespace pass {

/**
 * A pass that adds a constraint on the global parameters to the scop context.
 * Unlike GenClastPass::codegenContext, the constraint is visible to every
 * later pass (e.g. Pluto), not only to code generation.
 */
class ParamContextPass : public IPass {
  public:
	explicit ParamContextPass(ParamConstraint constraint);
	virtual ~ParamContextPass();

	virtual Bundle apply(Bundle &&bundle) override;
	virtual std::set<std::string> inputIR() const override {
		return {pesto::ir::OpenScopIR::getID()};
	};
	virtual std::string outputIR() const override {
		return {pesto::ir::OpenScopIR::getID()};
	};

  private:
	ParamConstraint constraint;
};

} // namespace pass

} // namespace pesto
