/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <pesto/Pluto.hpp>
#include <pesto/pass/Pass.hpp>

#include <isl/union_set.h>
#include <isl/union_map.h>

#include <pluto/pluto.h>

namespace pesto {

namespace pass {

/**
 * A generic pass that runs Pluto on each OslScop of the program
 */
#include <isl/ctx.h>
#include <isl/union_map.h>

class PlutoPass : public IPass {
  public:
	PlutoPass();

	virtual Bundle apply(Bundle &&bundle) override;
	virtual std::set<std::string> inputIR() const override;
	virtual std::string outputIR() const override;

	PlutoOptions &getOptions();
	void setCheckLegality(bool check) { this->checkLegality = check; }

  private:
	PlutoWrapper pluto;
	bool checkLegality = true;

	__isl_give isl_union_map *computeDeps(isl_ctx *ctx, osl_scop_p scop);
	__isl_give isl_union_map *violatedDeps(isl_ctx *ctx, osl_scop_p scop, __isl_keep isl_union_map *deps,
	                                       __isl_keep isl_union_set *origDomains, bool *reliable);
	/** Post-pluto schedule, re-expressed on the ORIGINAL (pre-pluto) statement
	 *  domains: { D_orig -> Scat }. Returns nullptr if the projection fails. */
	__isl_give isl_union_map *projectedSchedule(isl_ctx *ctx, osl_scop_p scop,
	                                            __isl_keep isl_union_set *origDomains, bool *reliable); 
	/** Outermost scattering dimension that is a real loop AND carries no dependence.
	 *  Returns -1 if there is none. Scattering dims are 0-based; the CLooG iterator
	 *  for dim d is "t<d+1>". */
	int findParallelDim(isl_ctx *ctx, osl_scop_p scop, __isl_keep isl_union_map *deps,
	                    __isl_keep isl_union_set *origDomains);                                      
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
