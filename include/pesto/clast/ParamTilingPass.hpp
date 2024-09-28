#ifndef PESTO_PARAM_TILING_PASS_H
#define PESTO_PARAM_TILING_PASS_H

#include <pesto/PolyhedralPass.hpp>

namespace pesto {

struct StmtGroup {
	int id;
	std::vector<struct clast_stmt *> stmts;
	struct clast_for *parent_for;
};

/**
 * A pass that performs a parametric tiling over the Clast of each ScopNest
 */
class ClastParamTilingPass : public IPolySingleNestPass {
  public:
	bool gen_trahrhe;

	ClastParamTilingPass();
	virtual ~ClastParamTilingPass();

	virtual void run(PolyProgram &prog);
	virtual void runSingleNest(ScopNest &nest);

  private:
	void applyTiling(ScopNest &nest, struct clast_stmt *root,
					 struct StmtGroup *group);
};

} // namespace pesto

#endif // PESTO_PARAM_TILING_PASS_H