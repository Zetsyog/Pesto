#ifndef PESTO_PASS_GEN_CLAST_H
#define PESTO_PASS_GEN_CLAST_H

#include <memory>
#include <string>
#include <vector>

#include <cloog/cloog.h>
#include <pesto/pass/Pass.hpp>

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
class GenClastPass : public IPass {
  public:
	int codegenContext;
	int cloogL;			  // cloog last optimization level
	int cloogF;			  // cloog first optimization level
	int cloogStop;		  // level to stop code generation
	int cloogSh;		  // 1 for computing simple hulls
	int cloogFirstUnroll; // The first dimension to unroll
	int cloogBacktrack;	  // 1 to use backtrack
	int cloogNoSimplify;  // 1 to not simplify the polyhedra
	int cloogEsp;

	GenClastPass();
	virtual ~GenClastPass();

	virtual void applyScop(OslScop &nest) override;
};

} // namespace pass

} // namespace pesto

#endif // PESTO_PASS_GEN_CLAST_H