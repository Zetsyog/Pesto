#ifndef PESTO_CODEGEN_PASSES_H
#define PESTO_CODEGEN_PASSES_H

#include <filesystem>
#include <string>
#include <vector>

#include <pesto/OslScop.hpp>
#include <pesto/Pass.hpp>
#include <pesto/Program.hpp>

namespace pesto {

namespace pass {

namespace metadata {

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
 * A metadata that holds a list of macros that will be included at the top of
 * the generated C code
 */
class MacroMetadata {
  public:
	std::vector<std::string> list;
};

} // namespace metadata

/**
 * A pass that generate C code from a Program
 * For each OslScop it will look for a ClastHolder, and use the clast to
 * generate C code
 * Else it will use cloog on the openscop representation of the OslScop
 */
class CloogCodegenPass : public IPass {
  public:
	CloogCodegenPass();
	virtual ~CloogCodegenPass();

	virtual void hookBegin(Program &prog) override;
	virtual void applyScop(OslScop &nest) override;

  private:
	std::filesystem::path output;

	pesto::pass::metadata::VarDeclMetadata mergeVarDecls(Program &prog);
};

} // namespace pass

} // namespace pesto

#endif // PESTO_CODEGEN_PASSES_H