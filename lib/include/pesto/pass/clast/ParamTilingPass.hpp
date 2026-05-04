/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <filesystem>
#include <optional>

#include <pesto/codegen/ParamTiling.hpp>

#include <pesto/ParamTilingUtility.hpp>
#include <pesto/pass/Pass.hpp>
#include <pesto/pass/PlutoPass.hpp>
#include <pesto/pass/clast/GenClastPass.hpp>

#include <isl/set.h>

#define PESTO_PARAM_TILING_DEFAULT_TILE_SIZE_HINT 32

namespace pesto {

namespace pass {

/**
 * A pass that performs a parametric tiling over the Clast of each OslScop
 */
class ClastParamTilingPass : public IPass {
  public:
	bool gen_trahrhe;
	bool trahrhe_use_dichotomy;

	ClastParamTilingPass();
	virtual ~ClastParamTilingPass();

	virtual Bundle apply(Bundle &&bundle) override;

	virtual std::set<std::string> inputIR() const override;
	virtual std::string outputIR() const override;

	/**
	 * @brief given a depth, return the tiling type
	 * @param depth the depth of the loop to tile
	 * @return the tiling type
	 */
	pesto::ParamTilingType getTilingType(size_t depth);

	/**
	 * @brief push a tiling type to the list of tiling types
	 * @param type the tiling type to push
	 */
	void pushTilingType(pesto::ParamTilingType type);

	void setTileSizeHint(int size);
	void setBoundComputeMethod(pesto::ptile::codegen::BoundComputeMethod method);
	void setBoundCorrectionStrategy(pesto::ptile::codegen::BoundCorrectionStrategy strategy);
	void setVolumeCheckStrategy(pesto::ptile::codegen::VolumeCheckStrategy strategy);
	void setVolumeCheckTrigger(pesto::VolumeCheckTrigger trigger);
	void setVolumeCheckThreshold(std::string threshold);

  private:
	std::unique_ptr<GenClastPass> gen_clast_pass;
	std::vector<pesto::ParamTilingType> tiling_types;
	ParamTilingUtility ptileUtil;

	bool areTileSkewed(osl_scop_p &scop);
	std::optional<std::pair<int, int>> getSkewedTilingDimensions(osl_statement_p stmt);

	void undoTileSkewing(osl_scop_p &scop);
};

} // namespace pass

} // namespace pesto
