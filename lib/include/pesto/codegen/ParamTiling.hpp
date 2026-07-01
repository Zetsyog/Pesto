/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <iostream>

#include <isl/set.h>

#include <cloog/cloog.h>

#include <pesto/Logging.hpp>

namespace pesto {

enum class ParamTilingType;
struct TilingData;
struct StmtGroup;

namespace ptile {

class NameHelper {
  public:
	NameHelper(int depth, std::string outerForIt, std::string innerForIt);
	virtual ~NameHelper();

	void setSeparator(std::string sep);
	void setPrefix(std::string prefix);
	std::string getPrefixStr();

	std::string &getSeparator();
	std::string &getPrefix();

	std::string outerForLBName();
	std::string outerForUBName();
	std::string innerForLBName();
	std::string innerForUBName();
	std::string divName();
	std::string targetVolName();
	std::string pcMaxName();
	std::string ehrhartFnName();
	std::string trahrheFnName();
	std::string trahrheIteratorName();
	std::string tileArrayName();
	std::string scheduleName();

	std::string innerForIteratorName();

  private:
	int depth;
	std::string sep_;
	std::string prefix_;
	std::string outerForIt_;
	std::string innerForIt_;
};

namespace codegen {

enum class StmtType {
	HEAD,
	BODY,
};

enum class BoundComputeMethod {
	FILL_LAST,
	FILL_FIRST,
};

enum class BoundCorrectionStrategy {
	NO_CORRECTION,
	SHOUT_AND_DIE,
	SHOUT_AND_CONTINUE,
	CORRECT,
};

enum class VolumeCheckStrategy {
	NO_CHECK,
	CHECK_AND_WARN,
	CHECK_AND_DIE,
	NO_TILE_UNTIL_THRESHOLD,
	DYNAMIC_TILE_UNTIL_THRESHOLD,
};

class ParamTilingCodegen {
  public:
	ParamTilingCodegen(StmtGroup &group, TilingData &data, int nestId, __isl_take isl_set *innerDomain);
	~ParamTilingCodegen();

	void genStmt(std::ostream &os, StmtType type);

	void setEndline(std::string endline);

	void setTilingType(ParamTilingType type);
	void setBoundComputeMethod(BoundComputeMethod method);
	void setBoundCorrectionStrategy(BoundCorrectionStrategy strategy);
	void setVolumeCheckStrategy(VolumeCheckStrategy strategy);
	void setVolumeCheckThreshold(std::string threshold);

	void genScheduleHeaderFile(std::ostream &os);

  private:
	std::shared_ptr<spdlog::logger> logger;

	StmtGroup &group;
	TilingData &data;
	ParamTilingType tilingType;
	struct clast_for &outerFor;
	std::size_t tilingDepth;
	isl_set *innerDomain;

	BoundComputeMethod boundComputeMethod;
	std::string endline;
	BoundCorrectionStrategy boundCorrectionStrategy;
	VolumeCheckStrategy volumeCheckStrategy;
	std::string volumeCheckThreshold;

	NameHelper &nameHelper;
	NameHelper &trahrheNameHelper;

	void genHeaderStmt(std::ostream &os);
	void genBodyStmt(std::ostream &os);

	void genHeaderStmtAlgebraic(std::ostream &os);
	void genBodyStmtAlgebraic(std::ostream &os);

	void genHeaderStmtRectangular(std::ostream &os);
	void genBodyStmtRectangular(std::ostream &os);

	void genHeaderStmtScheduled(std::ostream &os);
	void genBodyStmtScheduled(std::ostream &os);
};
} // namespace codegen

} // namespace ptile

} // namespace pesto
