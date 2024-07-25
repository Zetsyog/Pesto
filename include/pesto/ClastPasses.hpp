#ifndef PESTO_CLAST_PASSES_H
#define PESTO_CLAST_PASSES_H

#include <string>
#include <vector>

#include <pesto/PolyProgram.hpp>
#include <pesto/PolyhedralPass.hpp>
#include <pesto/ScopNest.hpp>

#include <cloog/cloog.h>

namespace pesto {

class IClastSingleNestPass : public IPolySingleNestPass {
  public:
	virtual ~IClastSingleNestPass();

	virtual void runSingleNest(ScopNest &nest);

	virtual std::string getUUID() = 0;
	virtual void runClast(struct clast_root *root) = 0;
};

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

#endif // PESTO_CLAST_PASSES_H