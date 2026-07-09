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

class MatchCriteriaPass : public IPass {
  public:
	MatchCriteriaPass();
	virtual ~MatchCriteriaPass();

	virtual Bundle apply(Bundle &&bundle) override;
	virtual std::set<std::string> inputIR() const override {
		return {pesto::ir::OpenScopIR::getID()};
	};
	virtual std::string outputIR() const override {
		return {pesto::ir::OpenScopIR::getID()};
	};

	void setCriteria(std::string criteria);
	void addThenPass(std::unique_ptr<IPass> pass);
	void addElsePass(std::unique_ptr<IPass> pass);

  private:
	std::string criteria;
	pesto::PassManager thenPassManager;
	pesto::PassManager elsePassManager;

	std::unordered_map<std::string, double> variables;
	mu::Parser parser;
};

} // namespace pass

} // namespace pesto
