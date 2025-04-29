#include <pesto/Pesto.hpp>

#include <pluto/osl_pluto.h>
#include <pluto/pluto.h>
#include <pluto/pluto_internal.h>
#include <pluto/program.h>

using namespace pesto;

PlutoWrapper::PlutoWrapper() {
	this->context = pluto_context_alloc();
	getOptions().tile_sizes_len = 1;
	getOptions().tile_sizes =
		(int *)malloc(getOptions().tile_sizes_len * sizeof(int));
	getOptions().tile_sizes[0] = 32; // default tile size is 32
}

PlutoWrapper::~PlutoWrapper() { pluto_context_free(context); }

void PlutoWrapper::run(osl_scop_p scop) {
	// TODO: should be this done in place, or should we create a new scop?
	PlutoProg *pluto_prog = osl_scop_to_pluto_prog(scop, context);
	pluto_schedule_prog(pluto_prog);
	pluto_populate_scop(scop, pluto_prog, context);

	pluto_prog_free(pluto_prog);
}

void PlutoWrapper::run(OslScop &oslScop) {
	osl_scop_p scop = oslScop.c_scop();
	run(scop);
}

PlutoWrapper &PlutoWrapper::tile(bool tile) {
	getOptions().tile = tile;
	return *this;
}

PlutoWrapper &PlutoWrapper::diamondTile(bool diamondTile) {
	getOptions().diamondtile = diamondTile;
	return *this;
}

PlutoWrapper &PlutoWrapper::parallel(bool parallel) {
	getOptions().parallel = parallel;
	return *this;
}

PlutoWrapper &PlutoWrapper::prevector(bool prevector) {
	getOptions().prevector = prevector;
	return *this;
}

PlutoWrapper &PlutoWrapper::unrollJam(bool unrollJam) {
	getOptions().unrolljam = unrollJam;
	return *this;
}

PlutoWrapper &PlutoWrapper::identity(bool identity) {
	getOptions().identity = identity;
	return *this;
}

PlutoWrapper &PlutoWrapper::islDep(bool islDep) {
	getOptions().isldep = islDep;
	return *this;
}

PlutoWrapper &PlutoWrapper::silent(bool silent) {
	getOptions().silent = silent;
	return *this;
}

PlutoWrapper &PlutoWrapper::quiet(bool quiet) {
	getOptions().quiet = quiet;
	return *this;
}

PlutoWrapper &PlutoWrapper::debug(bool debug) {
	getOptions().debug = debug;
	return *this;
}

PlutoWrapper &PlutoWrapper::moreDebug(bool moreDebug) {
	getOptions().moredebug = moreDebug;
	return *this;
}

PlutoContext &PlutoWrapper::getContext() { return *context; }

PlutoOptions &PlutoWrapper::getOptions() { return *(context->options); }

PlutoWrapper &PlutoWrapper::setTileSizes(std::initializer_list<int> sizes) {
	if (getOptions().tile_sizes != nullptr) {
		free(getOptions().tile_sizes);
	}
	getOptions().tile_sizes_len = sizes.size();
	getOptions().tile_sizes =
		(int *)malloc(getOptions().tile_sizes_len * sizeof(int));
	std::copy(sizes.begin(), sizes.end(), getOptions().tile_sizes);
	return *this;
}

PlutoWrapper &PlutoWrapper::pushTileSize(int size) {
	getOptions().tile_sizes_len += 1;
	getOptions().tile_sizes = (int *)realloc(
		getOptions().tile_sizes, getOptions().tile_sizes_len * sizeof(int));
	getOptions().tile_sizes[getOptions().tile_sizes_len - 1] = size;
	return *this;
}