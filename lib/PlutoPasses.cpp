#include <attila/Attila.hpp>
#include <iostream>

#include <isl/id.h>
#include <isl/map.h>
#include <isl/printer.h>
#include <isl/set.h>
#include <isl/space.h>
#include <isl/union_map.h>
#include <isl/union_set.h>
#include <osl/scop.h>
#include <pluto/pluto.h>
#include <string.h>

#include "isl_support.h"

PlutoAutoTransformPass::PlutoAutoTransformPass() {}

PlutoAutoTransformPass::~PlutoAutoTransformPass() {}

void PlutoAutoTransformPass::runSingleNest(ScopNest &nest) {
	PlutoContext *context = pluto_context_alloc();
	PlutoOptions *options = context->options;
	options->tile = 0;
	options->parallel = 0;
	options->prevector = 0;
	options->unrolljam = 0;
	options->isldep = 1;
	options->candldep = 0;

	osl_scop_p scop = nest.getScop();

	scop = pluto_schedule_osl(scop, context);

	int nparam = osl_scop_get_nb_parameters(scop);
	for (int i = 0; i < nparam; i += 1) {
		osl_relation_insert_blank_row(scop->context, scop->context->nb_rows);
		osl_int_set_long(scop->context->precision,
						 &scop->context->m[scop->context->nb_rows - 1][0], 1);
		osl_int_set_long(scop->context->precision,
						 &scop->context->m[scop->context->nb_rows - 1][1 + i],
						 1);
		// osl_int_set_long(
		// 	scop->context->precision,
		// 	&scop->context->m[scop->context->nb_rows - 1][1 + nparam], -1);
	}
}

std::string PlutoAutoTransformPass::getUUID() { return "pluto-auto-transform"; }