#include <iostream>
#include <pesto/Pesto.hpp>

#include <osl_support.h>

using namespace pesto::pass;

// ParamContextPass
ParamContextPass::ParamContextPass(int contextValue)
	: IPass("pass-param-context"), contextValue(contextValue) {}

ParamContextPass::~ParamContextPass() {}

void ParamContextPass::applyScop(OslScop &nest) {
	osl_scop_p scop = nest.getScop();
	int nparam = osl_scop_get_nb_parameters(scop);
	for (int i = 0; i < nparam; i += 1) {
		osl_relation_insert_blank_row(scop->context, scop->context->nb_rows);
		osl_int_set_long(scop->context->precision,
						 &scop->context->m[scop->context->nb_rows - 1][0], 1);
		osl_int_set_long(scop->context->precision,
						 &scop->context->m[scop->context->nb_rows - 1][1 + i],
						 1);
		osl_int_set_long(
			scop->context->precision,
			&scop->context->m[scop->context->nb_rows - 1][1 + nparam],
			-this->contextValue);
	}
}