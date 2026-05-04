/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <isl/set.h>

#include <osl/osl.h>

#include <cloog/cloog.h>

#include <pesto/Logging.hpp>
#include <pesto/codegen/ParamTiling.hpp>

namespace pesto {

enum class ParamTilingType { RECTANGULAR, ALGEBRAIC, SCHEDULED, LAST_DIM_RECT };

enum class VolumeCheckTrigger {
	NEVER,
	ALWAYS,
	FIRST_OUTTER_ITERATOR_DEPENDENCY,
};

/**
 * Holds information needed to generate trahrhe code
 * Associate outter tiling for to its corresponding inner tiling for
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

/**
 * A utility class to perform parametric tiling on a given program
 * using both the Polyhedral representation and the clast representation
 */
class ParamTilingUtility {
  public:
	ParamTilingUtility();
	virtual ~ParamTilingUtility();

	/**
	 * @brief Perform a parametric tiling on the given OslScop
	 * Tiling entry point. It uses both the Polyhedral representation and the
	 * clast representation
	 * @param nest[in] the OslScop to apply the tiling on
	 */
	void apply(CloogProgram *cloogProgram, CloogOptions *cloogOptions, struct clast_root *root);

	bool hasError();
	std::string getErrorMessage();

	/**
	 * @brief given a depth, return the tiling type
	 * @param depth the depth of the loop to tile
	 * @return the tiling type
	 */
	ParamTilingType getTilingType(std::size_t depth);

	/**
	 * @brief push a tiling type to the list of tiling types
	 * @param type the tiling type to push
	 */
	void pushTilingType(ParamTilingType type);

	/**
	 * @brief clear the list of tiling types
	 */
	void clearTilingTypes();

	void setIdTrahrhePrefix(int id);

	/* Codegen options */
	void setBoundComputingMethod(ptile::codegen::BoundComputeMethod method);
	void setBoundCorrectionStrategy(ptile::codegen::BoundCorrectionStrategy strategy);
	void setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy strategy);
	void setVolumeCheckTrigger(VolumeCheckTrigger trigger);
	void setVolumeCheckThreshold(std::string threshold);

	std::vector<std::shared_ptr<std::string>> getNames();

	std::vector<std::pair<int, std::string>> getStatements();

	std::vector<std::string> getVarList();

	std::vector<std::pair<int, std::string>> getTrahrheDomainList();

	void setTileSizeTarget(int tilesize);
	int getTileSizeTarget();

	std::optional<std::filesystem::path> getGeneratedScheduleHeaderFile();

  private:
	/* polyhedral stuff */
	isl_ctx *ctx;		  /* isl context */
	isl_printer *printer; /* isl printer, used to print isl objects to string */

	std::vector<ParamTilingType> tilingTypes;

	/* stmt utilities */
	int insertedStmts;
	int tilesizeTarget;

	/* error handling */
	bool error;
	std::string errorMessage;

	/* str utilities */
	std::vector<std::shared_ptr<std::string>> names; /* those string should survive till the end of pesto */

	std::vector<std::pair<int, std::string>> trahrheDomainList_; /* trahrhe domains */

	/* code gen */
	int id;
	std::vector<std::pair<int, std::string>> statements; /* list of statements id and their content */
	std::vector<std::string> varList;

	ptile::codegen::BoundComputeMethod boundComputeMethod;
	ptile::codegen::BoundCorrectionStrategy boundCorrectionStrategy;
	ptile::codegen::VolumeCheckStrategy volumeCheckStrategy;
	VolumeCheckTrigger volumeCheckTrigger;
	std::optional<std::string> volumeCheckThreshold;

	/* Logging */
	std::shared_ptr<spdlog::logger> logger;

	/* Methods */
	std::vector<std::unique_ptr<StmtGroup>> computeStmtGroups(CloogProgram *cloogProgram, struct clast_root *root);

	void tileGroup(CloogProgram *cloogProgram, CloogOptions *cloogOptions, struct clast_root *root, StmtGroup &group);

	void associateTilingFor(struct clast_root *root, StmtGroup &group);

	int islFirstTilingDimIdx(StmtGroup &group);

	void insertStatements(CloogOptions *cloogOptions, struct clast_root *root, TilingData &data);

	void replaceInnerForBounds(CloogProgram *cloogProgram, struct StmtGroup &group, TilingData &data);

	void replaceOutterForBounds(StmtGroup &group, TilingData &data);

	void generateStatements(CloogProgram *cloogProgram, StmtGroup &group);

	__isl_give isl_set *dropTilingConstraints(__isl_take isl_set *set, StmtGroup &group);

	__isl_give isl_set *moveNonTiledDimensionsToParameters(__isl_take isl_set *set, StmtGroup &group);

	void declareVariable(std::string varName);

	void declareGroupVariables(StmtGroup &group);

	void computeTrahrheDomain(CloogProgram *cloogProgram, StmtGroup &group);

	void updateOmpPragma(StmtGroup &group);

	__isl_give isl_set *removeTilingDimensions(StmtGroup &group, __isl_take isl_set *domain, int maxTilingDepth);

	ptile::codegen::ParamTilingCodegen getCodegen(CloogProgram *cloogProgram, StmtGroup &group, TilingData &data);

	void computeTilingDataBoundExpr(CloogProgram *cloogProgram, StmtGroup &group, TilingData &data);

	int computeDataTilingDepth(StmtGroup &group, TilingData &data);

	void computeUntiledDomain(CloogProgram *cloogProgram, StmtGroup &group);
};

} // namespace pesto
