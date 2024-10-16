#ifndef PESTO_CODEGEN_PASSES_H
#define PESTO_CODEGEN_PASSES_H

#include <pesto/Pass.hpp>
#include <pesto/PolyProgram.hpp>
#include <pesto/ScopNest.hpp>
#include <string>
#include <vector>

namespace pesto {

class VarDeclMetadata {
  public:
	VarDeclMetadata();
	virtual ~VarDeclMetadata();

	std::string type;

	void addVar(std::string var);
	std::vector<std::string> getVars();

  private:
	std::vector<std::string> vars;
};

/**
 * A pass that generate C code from a PolyProgram
 * For each ScopNest it will look for a ClastMetadata, and use the clast to
 * generate C code
 * Else it will use cloog on the openscop representation of the ScopNest
 */
class CloogCodegenPass : public IPestoSingleNestPass {
  public:
	CloogCodegenPass(std::string prefix);
	virtual ~CloogCodegenPass();

	virtual void run(PolyProgram &prog);
	virtual void runSingleNest(ScopNest &nest);

  private:
	std::string path_prefix;
};

class MacroMetadata {
  public:
	std::vector<std::string> list;
};

} // namespace pesto

#endif // PESTO_CODEGEN_PASSES_H