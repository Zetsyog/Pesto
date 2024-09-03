#ifndef PESTO_PARAM_TILING_PASS_H
#define PESTO_PARAM_TILING_PASS_H

#include <pesto/PolyhedralPass.hpp>

namespace pesto {

/**
 * A pass that performs a parametric tiling over the Clast of each ScopNest
 */
class ClastParamTilingPass : public IPolySingleNestPass {
  public:
	ClastParamTilingPass();
	virtual ~ClastParamTilingPass();

	virtual void run(PolyProgram &prog);
	virtual void runSingleNest(ScopNest &nest);
};

} // namespace pesto

#endif // PESTO_PARAM_TILING_PASS_H