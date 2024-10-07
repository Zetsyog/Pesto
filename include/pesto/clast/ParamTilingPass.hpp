#ifndef PESTO_PARAM_TILING_PASS_H
#define PESTO_PARAM_TILING_PASS_H

#include <pesto/Pass.hpp>

namespace pesto {

struct StmtGroup {
	int id;
	std::vector<struct clast_stmt *> stmts;
	struct clast_for *parent_for;
};

/**
 * A pass that performs a parametric tiling over the Clast of each ScopNest
 */
class ClastParamTilingPass : public IPestoSingleNestPass {
  public:
	bool gen_trahrhe;

	ClastParamTilingPass();
	virtual ~ClastParamTilingPass();

	virtual void run(PolyProgram &prog);
	virtual void runSingleNest(ScopNest &nest);

  private:
	bool is_stencil;
	void applyTiling(ScopNest &nest, struct clast_stmt *root,
					 struct StmtGroup *group);
};

} // namespace pesto

#endif // PESTO_PARAM_TILING_PASS_H