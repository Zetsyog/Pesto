#ifndef PESTO_CODEGEN_PASSES_H
#define PESTO_CODEGEN_PASSES_H

#include <pesto/PolyProgram.hpp>
#include <pesto/PolyhedralPass.hpp>
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

class CloogCodegenPass : public IPolySingleNestPass {
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