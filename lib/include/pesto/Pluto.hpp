/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <initializer_list>

#include <pluto/pluto.h>

#include <osl/osl.h>

namespace pesto {

/**
 * A class that wraps the Pluto polyhedral compiler
 */
class PlutoWrapper {
  public:
	PlutoWrapper();
	~PlutoWrapper();

	PlutoWrapper &tile(bool tile);
	PlutoWrapper &diamondTile(bool diamondTile);
	PlutoWrapper &parallel(bool parallel);
	PlutoWrapper &prevector(bool prevector);
	PlutoWrapper &unrollJam(bool unrollJam);
	PlutoWrapper &identity(bool identity);
	PlutoWrapper &islDep(bool islDep);
	PlutoWrapper &silent(bool silent);
	PlutoWrapper &quiet(bool quiet);
	PlutoWrapper &debug(bool debug);
	PlutoWrapper &moreDebug(bool moreDebug);

	/* Tile size utility */
	PlutoWrapper &setTileSizes(std::initializer_list<int> sizes);
	PlutoWrapper &setTileSizes(std::vector<int> sizes);
	PlutoWrapper &pushTileSize(int size);

	/**
	 * Run the Pluto polyhedral compiler on a given scop
	 * Optimisations are applied in place
	 * @param scop
	 */
	void run(osl_scop_p scop);

	/**
	 * Let the user access the C Pluto context
	 * @return PlutoContext
	 */
	PlutoContext &getContext();

	/**
	 * Let the user access the C Pluto options
	 * @return PlutoOptions
	 */
	PlutoOptions &getOptions();

  private:
	PlutoContext *context;
};

} // namespace pesto
