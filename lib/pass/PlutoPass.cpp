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

using namespace pesto::pass;

PlutoPass::PlutoPass() : ISinglePass("pass-pluto"), pluto() {
	this->pluto = this->pluto.silent(!PLUTO_DEBUG)
					  .quiet(!PLUTO_DEBUG)
					  .debug(PLUTO_DEBUG)
					  .moreDebug(PLUTO_DEBUG)
					  .identity(0)
					  .islDep(1)
					  .tile(0)
					  .diamondTile(0)
					  .parallel(0)
					  .prevector(0)
					  .unrollJam(0);
}

PlutoOptions &PlutoPass::getOptions() { return this->pluto.getOptions(); }

void PlutoPass::runSingleNest(OslScop &nest) { this->pluto.run(nest); }

PlutoAutoTransformPass::PlutoAutoTransformPass() : PlutoPass() {
	this->name = "pass-pluto-auto-transform";
}

PlutoTilingPass::PlutoTilingPass() : PlutoPass() {
	// pluto opt
	this->getOptions().tile = 1;

	this->name = "pass-pluto-tiling";
}
