#ifndef PESTO_GEN_CLAST_PASS_H
#define PESTO_GEN_CLAST_PASS_H

#include <memory>
#include <string>
#include <vector>

#include <cloog/cloog.h>
#include <pesto/Pass.hpp>

namespace pesto {

/**
 * A metadata that holds a clast generated from an openscop representation
 */
class ClastMetadata {
  public:
	ClastMetadata();
	virtual ~ClastMetadata();

	/* Cloog variable */
	CloogState *cloogState;
	CloogOptions *cloogOptions;
	CloogProgram *cloogProgram;

	unsigned stmtsNb;
	struct clast_stmt *root;
	std::vector<std::shared_ptr<std::string>> names;
};

/**
 * A pass that generates a clast from an openscop representation
 * The clast is stored in a ClastMetadata for each ScopNest
 */
class GenClastPass : public IPestoSingleNestPass {
  public:
	int codegenContext;

	GenClastPass();
	virtual ~GenClastPass();

	virtual void runSingleNest(ScopNest &nest);
};

} // namespace pesto

#endif // PESTO_GEN_CLAST_PASS_H