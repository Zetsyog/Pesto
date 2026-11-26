/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/pass/ParamContext.hpp>
#include <pesto/pass/Pass.hpp>

using namespace pesto::pass;
using namespace pesto;

// ParamContextPass
ParamContextPass::ParamContextPass(ParamConstraint constraint)
	: IPass("param-context"), constraint(std::move(constraint)) {}

ParamContextPass::~ParamContextPass() {}

Bundle ParamContextPass::apply(Bundle &&bundle) {
	auto *scopIR = dynamic_cast<pesto::ir::OpenScopIR *>(bundle.get_ir());
	applyParamConstraint(scopIR->c_scop(), this->constraint);
	return std::move(bundle);
}
