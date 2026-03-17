/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#ifndef PESTO_PASS_PLUTO_H
#define PESTO_PASS_PLUTO_H

#include <pesto/OslScop.hpp>
#include <pesto/Pluto.hpp>
#include <pesto/Program.hpp>
#include <pesto/pass/Pass.hpp>

#include <pluto/pluto.h>

namespace pesto {

namespace pass {

/**
 * A generic pass that runs Pluto on each OslScop of the program
 */
class PlutoPass : public IPass {
  public:
	PlutoPass();

	virtual void applyScop(OslScop &nest) override;

	PlutoOptions &getOptions();

  private:
	PlutoWrapper pluto;
};

/**
 * A pass that runs Pluto without any optimization option
 * like polycc --notile --noparallel --nounroll --noprevector
 */
class PlutoAutoTransformPass : public PlutoPass {
  public:
	PlutoAutoTransformPass();
};

/**
 * A pass that runs Pluto with only the tiling option
 * like polycc --tile
 */
class PlutoTilingPass : public PlutoPass {
  public:
	PlutoTilingPass();
};

} // namespace pass

} // namespace pesto

#endif // PESTO_PASS_PLUTO_H