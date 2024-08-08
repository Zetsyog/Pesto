#ifndef PESTO_GEN_CLAST_PASS_H
#define PESTO_GEN_CLAST_PASS_H

#include <memory>
#include <string>
#include <vector>

#include <cloog/cloog.h>
#include <pesto/PolyhedralPass.hpp>

namespace pesto {

class ClastMetadata {
  public:
	ClastMetadata();
	virtual ~ClastMetadata();

	CloogState *cloogState;
	CloogOptions *cloogOptions;
	CloogProgram *cloogProgram;
	unsigned stmtsNb;
	struct clast_stmt *root;
	std::vector<std::shared_ptr<std::string>> names;
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