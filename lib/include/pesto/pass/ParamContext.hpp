/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <pesto/PassManager.hpp>
#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/pass/Pass.hpp>

#include <map>
#include <unordered_map>

#include <muParser.h>

namespace pesto {

namespace pass {

/**
 * A pass that adds a minimal value to global parameters of each scop of the
 * program
 */
class ParamContextPass : public IPass {
  public:
	ParamContextPass(int contextValue);
	virtual ~ParamContextPass();

	virtual Bundle apply(Bundle &&bundle) override;
	virtual std::set<std::string> inputIR() const override {
		return {pesto::ir::OpenScopIR::getID()};
	};
	virtual std::string outputIR() const override {
		return {pesto::ir::OpenScopIR::getID()};
	};

  private:
	int contextValue;
};

} // namespace pass

} // namespace pesto
