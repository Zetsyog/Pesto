#ifndef PESTO_TRAHRHE_CODEGEN_H
#define PESTO_TRAHRHE_CODEGEN_H

#include <iostream>

#include <isl/set.h>

#include <cloog/cloog.h>

namespace pesto {

enum class ParamTilingType;
struct TilingData;
struct StmtGroup;

namespace ptile {

class NameHelper {
  public:
	NameHelper(int depth, std::string outterForIt, std::string innerForIt);
	virtual ~NameHelper();

	void setSeparator(std::string sep);
	void setPrefix(std::string prefix);

	std::string &getSeparator();
	std::string &getPrefix();

	std::string outterForUBName();
	std::string innerForLBName();
	std::string innerForUBName();
	std::string divName();
	std::string targetVolName();
	std::string pcMaxName();
	std::string ehrhartFnName();
	std::string trahrheFnName();
	std::string trahrheIteratorName();

  private:
	int depth;
	std::string sep;
	std::string prefix;
	std::string outterForIt;
	std::string innerForIt;
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
};

class ParamTilingCodegen {
  public:
	ParamTilingCodegen(StmtGroup &group, struct clast_for &outterFor, struct clast_for &innerFor, int tilingDepth,
					   int nestId, __isl_take isl_set *innerDomain);
	~ParamTilingCodegen();

	void genStmt(std::ostream &os, StmtType type);

	void setEndline(std::string endline);

	void setTilingType(ParamTilingType type);
	void setBoundComputeMethod(BoundComputeMethod method);
	void setBoundCorrectionStrategy(BoundCorrectionStrategy strategy);
	void setVolumeCheckStrategy(VolumeCheckStrategy strategy);

  private:
	StmtGroup &group;
	ParamTilingType tilingType;
	struct clast_for &outterFor;
	struct clast_for &innerFor;
	std::size_t tilingDepth;
	isl_set *innerDomain;

	BoundComputeMethod boundComputeMethod;
	std::string endline;
	BoundCorrectionStrategy boundCorrectionStrategy;
	VolumeCheckStrategy volumeCheckStrategy;

	/* Names */
	std::string innerForLBExpr;
	std::string innerForUBExpr;

	NameHelper nameHelper;
	NameHelper trahrheNameHelper;

	void computeBoundExpr();

	void genHeaderStmt(std::ostream &os);
	void genHeaderStmtAlgebraic(std::ostream &os);
	void genHeaderStmtRectangular(std::ostream &os);
	void genBodyStmt(std::ostream &os);
	void genBodyStmtAlgebraic(std::ostream &os);
	void genBodyStmtRectangular(std::ostream &os);
};
} // namespace codegen

} // namespace ptile

} // namespace pesto

#endif // PESTO_TRAHRHE_CODEGEN_H