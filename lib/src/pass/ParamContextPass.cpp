/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/osl_support.h>
#include <pesto/pass/OslPass.hpp>
#include <pesto/pass/Pass.hpp>

using namespace pesto::pass;
using namespace pesto;

// ParamContextPass
ParamContextPass::ParamContextPass(int contextValue) : IPass("param-context"), contextValue(contextValue) {}

ParamContextPass::~ParamContextPass() {}

Bundle ParamContextPass::apply(Bundle &&bundle) {
	auto *scopIR = dynamic_cast<pesto::ir::OpenScopIR *>(bundle.get_ir());
	osl_scop_p scop = scopIR->c_scop();
	int nparam = osl_scop_get_nb_parameters(scop);
	for (int i = 0; i < nparam; i += 1) {
		osl_relation_insert_blank_row(scop->context, scop->context->nb_rows);
		osl_int_set_long(scop->context->precision, &scop->context->m[scop->context->nb_rows - 1][0], 1);
		osl_int_set_long(scop->context->precision, &scop->context->m[scop->context->nb_rows - 1][1 + i], 1);
		osl_int_set_long(scop->context->precision, &scop->context->m[scop->context->nb_rows - 1][1 + nparam],
						 -this->contextValue);
	}
	return std::move(bundle);
}