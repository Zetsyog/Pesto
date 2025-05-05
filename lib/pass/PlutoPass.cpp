#include <iostream>

#include <isl/id.h>
#include <isl/map.h>
#include <isl/printer.h>
#include <isl/set.h>
#include <isl/space.h>
#include <isl/union_map.h>
#include <isl/union_set.h>

#include <osl/scop.h>

#include <pluto/osl_pluto.h>
#include <pluto/pluto.h>
#include <pluto/pluto_internal.h>
#include <pluto/program.h>

#include <pesto/Pesto.hpp>
#include <pesto/isl_support.h>

#define PLUTO_DEBUG 0
#define PLUTO_QUIET 0

using namespace pesto::pass;

PlutoPass::PlutoPass() : IPass("pass-pluto"), pluto() {
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

void PlutoPass::applyScop(OslScop &nest) {
	log(std::cerr) << "Applying pluto on nest " << nest.getId() << std::endl;
	// run the pass on each nest
	this->pluto.run(nest);
}

PlutoAutoTransformPass::PlutoAutoTransformPass() : PlutoPass() {
	this->name = "pass-pluto-auto-transform";
}

PlutoTilingPass::PlutoTilingPass() : PlutoPass() {
	// pluto opt
	this->getOptions().tile = 1;

	this->name = "pass-pluto-tiling";
}
