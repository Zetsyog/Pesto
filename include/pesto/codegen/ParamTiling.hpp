#ifndef PESTO_TRAHRHE_CODEGEN_H
#define PESTO_TRAHRHE_CODEGEN_H

#include <iostream>

#include <isl/set.h>

#include <cloog/cloog.h>

namespace pesto {

enum class ParamTilingType;
struct TilingData;
struct StmtGroup;

namespace ptile::codegen {

enum StmtType { HEAD, BODY };

enum BoundComputeMethod {
	FILL_LAST,
	FILL_FIRST,
};

// TODO: better name?
class ParamTilingCodegen {
  public:
	ParamTilingCodegen(StmtGroup &group, struct clast_for &outterFor, struct clast_for &innerFor, int tilingDepth,
					   int nestId, isl_set *innerDomain);
	~ParamTilingCodegen();

	void genStmt(std::ostream &os, StmtType type);

	void setTilingType(ParamTilingType type);
	void setBoundComputeMethod(BoundComputeMethod method);

	std::string &getGroupPrefixName();
	std::string &getNestPrefixName();
	std::string &getOutterForUBName();
	std::string &getInnerForLBName();
	std::string &getInnerForUBName();
	std::string &getDivName();
	std::string &getTargetVolName();
	std::string &getPCMaxName();
	std::string &getEhrhartFnName();
	std::string &getTrahrheFnName();
	std::string &getTrahrheIteratorName();

  private:
	StmtGroup &group;
	ParamTilingType tilingType;
	struct clast_for &outterFor;
	struct clast_for &innerFor;
	std::size_t tilingDepth;
	int nestId;
	isl_set *innerDomain;

	BoundComputeMethod boundComputeMethod;
	std::string endline;

	/* Names */
	std::string groupPrefixName;
	std::string nestPrefixName;
	std::string outterForUBName;
	std::string innerForLBName;
	std::string innerForUBName;
	std::string divName;
	std::string targetVolName;
	std::string pcMaxName;
	std::string ehrhartFnName;
	std::string trahrheFnName;
	std::string trahrheIteratorName;

	std::string innerForLBExpr;
	std::string innerForUBExpr;

	void computeNames();

	void genHeaderStmt(std::ostream &os);
	void genHeaderStmtAlgebraic(std::ostream &os);
	void genHeaderStmtRectangular(std::ostream &os);
	void genBodyStmt(std::ostream &os);
	void genBodyStmtAlgebraic(std::ostream &os);
	void genBodyStmtRectangular(std::ostream &os);
};

} // namespace ptile::codegen

} // namespace pesto

#endif // PESTO_TRAHRHE_CODEGEN_H