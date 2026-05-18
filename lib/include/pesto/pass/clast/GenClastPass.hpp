/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <cloog/cloog.h>

#include <pesto/ir/ClastIR.hpp>
#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/pass/Pass.hpp>

namespace pesto {

namespace pass {

/**
 * A pass that generates a clast from an openscop representation
 * The clast is stored in a ClastHolder for each OslScop
 */
class GenClastPass : public IPass {
  public:
	int codegenContext;
	int cloogL;			  // cloog last optimization level
	int cloogF;			  // cloog first optimization level
	int cloogStop;		  // level to stop code generation
	int cloogSh;		  // 1 for computing simple hulls
	int cloogFirstUnroll; // The first dimension to unroll
	int cloogBacktrack;	  // 1 to use backtrack
	int cloogNoSimplify;  // 1 to not simplify the polyhedra
	int cloogEsp;

	GenClastPass();
	virtual ~GenClastPass();

	virtual Bundle apply(Bundle &&bundle) override;
	virtual std::set<std::string> inputIR() const override {
		return {pesto::ir::OpenScopIR::getID()};
	};
	virtual std::string outputIR() const override {
		return pesto::ir::ClastIR::getID();
	}
};

} // namespace pass

} // namespace pesto
