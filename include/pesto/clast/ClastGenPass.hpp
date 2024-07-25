#ifndef PESTO_GEN_CLAST_PASS_H
#define PESTO_GEN_CLAST_PASS_H

#include <string>

#include <pesto/clast/ClastPass.hpp>

#include <cloog/cloog.h>

namespace pesto {

class ClastMetadata {
  public:
	CloogState *cloogState;
	CloogOptions *cloogOptions;
	struct clast_stmt *root;
};

class ClastGenPass : public IPolySingleNestPass {
  public:
	ClastGenPass();
	virtual ~ClastGenPass();

	virtual std::string getUUID();
	virtual void runSingleNest(ScopNest &nest);
};

} // namespace pesto

#endif // PESTO_GEN_CLAST_PASS_H