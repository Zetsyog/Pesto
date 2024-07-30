#ifndef PESTO_GEN_CLAST_PASS_H
#define PESTO_GEN_CLAST_PASS_H

#include <string>

#include <cloog/cloog.h>
#include <pesto/PolyhedralPass.hpp>

namespace pesto {

class ClastMetadata {
  public:
	CloogState *cloogState;
	CloogOptions *cloogOptions;
	struct clast_stmt *root;
};

class GenClastPass : public IPolySingleNestPass {
  public:
	GenClastPass();
	virtual ~GenClastPass();

	virtual std::string getUUID();
	virtual void runSingleNest(ScopNest &nest);
};

} // namespace pesto

#endif // PESTO_GEN_CLAST_PASS_H