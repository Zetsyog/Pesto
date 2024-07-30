#ifndef PESTO_PARAM_TILING_PASS_H
#define PESTO_PARAM_TILING_PASS_H

#include <pesto/PolyhedralPass.hpp>

namespace pesto {

class ClastParamTilingPass : public IPolySingleNestPass {
  public:
	ClastParamTilingPass();
	virtual ~ClastParamTilingPass();

	virtual void run(PolyProgram &prog);
	virtual void runSingleNest(ScopNest &nest);

	virtual std::string getUUID();
};

} // namespace pesto

#endif // PESTO_PARAM_TILING_PASS_H