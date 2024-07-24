#include <iostream>
#include <pesto/Pesto.hpp>

#include <isl/id.h>
#include <isl/map.h>
#include <isl/printer.h>
#include <isl/set.h>
#include <isl/space.h>
#include <isl/union_map.h>
#include <isl/union_set.h>
#include <osl/scop.h>
#include <pluto/internal/pluto.h>
#include <pluto/osl_pluto.h>
#include <pluto/pluto.h>
#include <string.h>

#include "isl_support.h"

#define PLUTO_DEBUG 0
#define PLUTO_QUIET 0

PlutoPass::PlutoPass(std::string suffix) {
	this->suffix = suffix;
	this->context = pluto_context_alloc();
}

PlutoOptions *PlutoPass::getOptions() { return this->context->options; }

void PlutoPass::runSingleNest(ScopNest &nest) {
	osl_scop_p scop = nest.getScop();

	PlutoProg *pluto_prog = osl_scop_to_pluto_prog(scop, context);
	pluto_schedule_prog(pluto_prog);
	pluto_populate_scop(scop, pluto_prog, context);

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
			&scop->context->m[scop->context->nb_rows - 1][1 + nparam], -1);
	}
}

PlutoPass::~PlutoPass() { pluto_context_free(this->context); }

std::string PlutoPass::getUUID() { return "pluto-" + this->suffix; }

PlutoAutoTransformPass::PlutoAutoTransformPass() : PlutoPass("auto-transform") {
	this->getOptions()->silent = !PLUTO_DEBUG;
	this->getOptions()->quiet = !PLUTO_DEBUG;
	this->getOptions()->debug = PLUTO_DEBUG;
	this->getOptions()->moredebug = PLUTO_DEBUG;

	this->getOptions()->identity = 0;
	this->getOptions()->isldep = 1;
	this->getOptions()->readscop = 1;

	// pluto opt
	this->getOptions()->tile = 0;
	this->getOptions()->diamondtile = 0;
	this->getOptions()->parallel = 0;
	this->getOptions()->prevector = 0;
	this->getOptions()->unrolljam = 0;
}

PlutoAutoTransformPass::~PlutoAutoTransformPass() {}

PlutoTilingPass::PlutoTilingPass() : PlutoPass("tiling") {
	this->getOptions()->silent = PLUTO_QUIET;
	this->getOptions()->quiet = PLUTO_QUIET;
	this->getOptions()->debug = PLUTO_DEBUG;
	this->getOptions()->moredebug = PLUTO_DEBUG;

	this->getOptions()->identity = 0;
	this->getOptions()->isldep = 1;
	this->getOptions()->readscop = 1;

	// pluto opt
	this->getOptions()->tile = 1;
	this->getOptions()->diamondtile = 0;
	this->getOptions()->parallel = 1;
	this->getOptions()->prevector = 0;
	this->getOptions()->unrolljam = 0;
}
PlutoTilingPass::~PlutoTilingPass() {}
