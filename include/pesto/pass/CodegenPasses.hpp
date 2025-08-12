#ifndef PESTO_CODEGEN_PASSES_H
#define PESTO_CODEGEN_PASSES_H

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <pesto/OslScop.hpp>
#include <pesto/Program.hpp>
#include <pesto/pass/Pass.hpp>
#include <pesto/pass/clast/GenClastPass.hpp>

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

	MacroMetadata();
	virtual ~MacroMetadata();

	void define(std::string name, std::string value);
	void defineFunc(std::string name, std::string args, std::string value);

	std::vector<std::string> getDefines() { return this->defines; }

  private:
	std::vector<std::string> defines;
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

	virtual void apply(Program &prog) override;
	virtual void applyScop(OslScop &nest) override;

	void setGenClastPass(std::unique_ptr<pass::GenClastPass> pass);

  private:
	std::filesystem::path output;

	std::unique_ptr<pass::GenClastPass> genClastPass;

	pesto::pass::metadata::VarDeclMetadata mergeVarDecls(Program &prog);
};

} // namespace pass

} // namespace pesto

#endif // PESTO_CODEGEN_PASSES_H