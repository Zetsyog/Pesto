/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pluto/osl_pluto.h>
#include <pluto/pluto.h>
#include <pluto/pluto_internal.h>
#include <pluto/program.h>

#include <pesto/Pluto.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace pesto;

PlutoWrapper::PlutoWrapper() {
	this->context = pluto_context_alloc();
	this->setTileSizes({32, 32, 32});
}

PlutoWrapper::~PlutoWrapper() {
	pluto_context_free(context);
}

void PlutoWrapper::run(osl_scop_p scop) {
	// Pluto's OSL importer exits the process on these relations. Reject them
	// at the library boundary so callers get an actionable, catchable error.
	auto check = [](osl_relation_p relation) {
		for (; relation; relation = relation->next) {
			if (relation->nb_local_dims > 0) {
				throw std::invalid_argument(
					"Pluto does not support local dimensions (including non-unit loop strides)");
			}
		}
	};
	for (auto *stmt = scop->statement; stmt; stmt = stmt->next) {
		check(stmt->domain);
		check(stmt->scattering);
		for (auto *access = stmt->access; access; access = access->next)
			check(access->elt);
	}
	pluto_options_validate(context->options);
	// TODO: should be this done in place, or should we create a new scop?
	PlutoProg *pluto_prog = osl_scop_to_pluto_prog(scop, context);
	pluto_schedule_prog(pluto_prog);
	pluto_populate_scop(scop, pluto_prog, context);

	pluto_prog_free(pluto_prog);
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

PlutoContext &PlutoWrapper::getContext() {
	return *context;
}

PlutoOptions &PlutoWrapper::getOptions() {
	return *(context->options);
}

PlutoWrapper &PlutoWrapper::setTileSizes(std::initializer_list<int> sizes) {
	return setTileSizes(std::vector<int>(sizes));
}

PlutoWrapper &PlutoWrapper::setTileSizes(std::vector<int> sizes) {
	if (sizes.empty() || std::any_of(sizes.begin(), sizes.end(), [](int size) { return size <= 0; })) {
		throw std::invalid_argument("Tile sizes must be a nonempty list of strictly positive integers");
	}
	if (getOptions().tile_sizes != nullptr) {
		free(getOptions().tile_sizes);
	}
	getOptions().tile_sizes_len = sizes.size();
	getOptions().tile_sizes = (int *)malloc(getOptions().tile_sizes_len * sizeof(int));
	std::copy(sizes.begin(), sizes.end(), getOptions().tile_sizes);
	return *this;
}

PlutoWrapper &PlutoWrapper::pushTileSize(int size) {
	if (size <= 0) {
		throw std::invalid_argument("Tile sizes must be strictly positive integers");
	}
	getOptions().tile_sizes_len += 1;
	getOptions().tile_sizes = (int *)realloc(getOptions().tile_sizes, getOptions().tile_sizes_len * sizeof(int));
	getOptions().tile_sizes[getOptions().tile_sizes_len - 1] = size;
	return *this;
}
