#include <iostream>
#include <string.h>

#include <isl/id.h>
#include <isl/map.h>
#include <isl/printer.h>
#include <isl/set.h>
#include <isl/space.h>
#include <isl/union_map.h>
#include <isl/union_set.h>

#include <osl/scop.h>

#include <pluto/internal/pluto.h>
#include <pluto/internal/program.h>
#include <pluto/osl_pluto.h>
#include <pluto/pluto.h>

#include "isl_support.h"
#include "pesto/Pesto.hpp"

#define PLUTO_DEBUG 0
#define PLUTO_QUIET 0

using namespace pesto;

PlutoPass::PlutoPass() { this->context = pluto_context_alloc(); }

PlutoOptions *PlutoPass::getOptions() { return this->context->options; }

void PlutoPass::runSingleNest(ScopNest &nest) {
	osl_scop_p scop = nest.getScop();

	PlutoProg *pluto_prog = osl_scop_to_pluto_prog(scop, context);
	pluto_schedule_prog(pluto_prog);
	pluto_populate_scop(scop, pluto_prog, context);

	pluto_prog_free(pluto_prog);
}

PlutoPass::~PlutoPass() { pluto_context_free(this->context); }

PlutoAutoTransformPass::PlutoAutoTransformPass() {
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

std::string PlutoAutoTransformPass::getUUID() {
	return PESTO_UUID_PASS_PLUTO_AUTO_TRANSFORM;
}

PlutoTilingPass::PlutoTilingPass() {
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
	this->getOptions()->parallel = 0;
	this->getOptions()->prevector = 0;
	this->getOptions()->unrolljam = 0;
}
PlutoTilingPass::~PlutoTilingPass() {}

std::string PlutoTilingPass::getUUID() { return PESTO_UUID_PASS_PLUTO_TILING; }
