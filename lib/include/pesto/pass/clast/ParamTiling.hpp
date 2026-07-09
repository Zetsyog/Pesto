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

#include <pesto/pass/Pass.hpp>
#include <pesto/pass/Pluto.hpp>
#include <pesto/pass/clast/GenClast.hpp>

#include <isl/set.h>

#define PESTO_PARAM_TILING_DEFAULT_TILE_SIZE_HINT 32

namespace pesto {

enum class ParamTilingType { RECTANGULAR, ALGEBRAIC, SCHEDULED, LAST_DIM_RECT };

enum class VolumeCheckTrigger {
	NEVER,
	ALWAYS,
	FIRST_OUTER_ITERATOR_DEPENDENCY,
};

/**
 * Holds information needed to generate trahrhe code
 * Associate outer tiling for to its corresponding inner tiling for
 * Also holds the tiling depth and the codegen data
 */
struct TilingData {
	int id;
	struct clast_for *outer_for;
	struct clast_for *inner_for;
	int tiling_depth;
	unsigned int head_stmt_id;
	unsigned int body_stmt_id;
	std::string innerForLBExpr;
	std::string innerForUBExpr;

	// codegen helper
	std::unique_ptr<ptile::NameHelper> nameHelper;
	std::unique_ptr<ptile::NameHelper> trahrheNameHelper;
};

struct StmtGroup {
  public:
	int id;
	std::vector<struct clast_user_stmt *> stmts;
	/**
	 * The parent for of the statements in the group
	 */
	struct clast_for *stmt_parent_for;

	/**
	 * The first inner for of the group
	 */
	struct clast_for *first_inner_for;
	/**
	 * The outermost for of the group
	 * It is the loop iterating over tiles common to all statements in the group
	 */
	struct clast_for *outermost_for;

	/**
	 * The number of dimensions that have been moved to the tiling space
	 */
	size_t movedDimensionsCount = 0;
	/**
	 * The domain representing the inner loop nest, without the tiling dimensions
	 * This will be used to generate trahrhe expressions, and compute back original loop bounds
	 */
	isl_set *untiled_domain;

	/**
	 * A vector of TilingData
	 */
	std::vector<std::unique_ptr<TilingData>> tilingData;

	/**
	 * Holds domain information to pass to trahrhe software
	 * First element is the number of algebraically tiled dimensions
	 * Second element is the domain in isl string format
	 */
	std::vector<std::pair<int, std::string>> trahrheDomains;

	~StmtGroup();
};

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

	void setGenClastPass(std::unique_ptr<GenClastPass> pass);

	/**
	 * @brief clear the list of tiling types
	 */
	void clearTilingTypes();

	void setBoundComputeMethod(pesto::ptile::codegen::BoundComputeMethod method);
	void setBoundCorrectionStrategy(pesto::ptile::codegen::BoundCorrectionStrategy strategy);
	void setVolumeCheckStrategy(pesto::ptile::codegen::VolumeCheckStrategy strategy);
	void setVolumeCheckTrigger(pesto::VolumeCheckTrigger trigger);
	void setVolumeCheckThreshold(std::string threshold);

  private:
	/* polyhedral stuff */
	isl_ctx *ctx;		  /* isl context */
	isl_printer *printer; /* isl printer, used to print isl objects to string */

	std::vector<ParamTilingType> tilingTypes;

	/* stmt utilities */
	int insertedStmts;

	/* str utilities */
	std::vector<std::shared_ptr<std::string>> names; /* those string should survive till the end of pesto */

	std::vector<std::pair<int, std::string>> trahrheDomainList_; /* trahrhe domains */

	/* code gen */
	int id;
	std::vector<std::pair<int, std::string>> statements; /* list of statements id and their content */
	std::vector<std::string> varList;

	pesto::ptile::codegen::BoundComputeMethod boundComputeMethod;
	pesto::ptile::codegen::BoundCorrectionStrategy boundCorrectionStrategy;
	pesto::ptile::codegen::VolumeCheckStrategy volumeCheckStrategy;
	VolumeCheckTrigger volumeCheckTrigger;
	std::optional<std::string> volumeCheckThreshold;

	std::unique_ptr<GenClastPass> gen_clast_pass;

	bool areTileSkewed(osl_scop_p &scop);
	std::optional<std::pair<int, int>> getSkewedTilingDimensions(osl_statement_p stmt);

	void undoTileSkewing(osl_scop_p &scop);

	/* param tiling methods */
	std::vector<std::unique_ptr<StmtGroup>> computeStmtGroups(CloogProgram *cloogProgram, struct clast_root *root);

	void tileGroup(CloogProgram *cloogProgram, CloogOptions *cloogOptions, struct clast_root *root, StmtGroup &group);

	void associateTilingFor(struct clast_root *root, StmtGroup &group);

	int islFirstTilingDimIdx(StmtGroup &group);

	void insertStatements(CloogOptions *cloogOptions, struct clast_root *root, TilingData &data);

	void replaceInnerForBounds(CloogProgram *cloogProgram, struct StmtGroup &group, TilingData &data);

	void replaceOuterForBounds(StmtGroup &group, TilingData &data);

	void generateStatements(CloogProgram *cloogProgram, StmtGroup &group);

	__isl_give isl_set *dropTilingConstraints(__isl_take isl_set *set, StmtGroup &group);

	__isl_give isl_set *moveNonTiledDimensionsToParameters(__isl_take isl_set *set, StmtGroup &group);

	void declareVariable(std::string varName);

	void declareGroupVariables(StmtGroup &group);

	void computeTrahrheDomain(CloogProgram *cloogProgram, StmtGroup &group);

	void updateOmpPragma(StmtGroup &group);

	__isl_give isl_set *removeTilingDimensions(StmtGroup &group, __isl_take isl_set *domain, int maxTilingDepth);

	pesto::ptile::codegen::ParamTilingCodegen getCodegen(CloogProgram *cloogProgram, StmtGroup &group,
														 TilingData &data);

	void computeTilingDataBoundExpr(CloogProgram *cloogProgram, StmtGroup &group, TilingData &data);

	int computeDataTilingDepth(StmtGroup &group, TilingData &data);

	void computeUntiledDomain(CloogProgram *cloogProgram, StmtGroup &group);
};

} // namespace pass

} // namespace pesto
