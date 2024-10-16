#ifndef PESTO_PASS_GEN_CLAST_H
#define PESTO_PASS_GEN_CLAST_H

#include <memory>
#include <string>
#include <vector>

#include <cloog/cloog.h>
#include <pesto/Pass.hpp>

namespace pesto {

namespace pass {

namespace metadata {
/**
 * A metadata that holds a clast generated from an openscop representation
 */
class ClastHolder {
  public:
	ClastHolder();
	virtual ~ClastHolder();

	/* Cloog variable */
	CloogState *cloogState;
	CloogOptions *cloogOptions;
	CloogProgram *cloogProgram;

	unsigned stmtsNb;
	struct clast_stmt *root;
	std::vector<std::shared_ptr<std::string>> names;
};

} // namespace metadata

/**
 * A pass that generates a clast from an openscop representation
 * The clast is stored in a ClastHolder for each OslScop
 */
class GenClastPass : public ISinglePass {
  public:
	int codegenContext;

	GenClastPass();
	virtual ~GenClastPass();

	virtual void runSingleNest(OslScop &nest);
};

} // namespace pass

} // namespace pesto

#endif // PESTO_PASS_GEN_CLAST_H