#ifndef PESTO_CODEGEN_PASSES_H
#define PESTO_CODEGEN_PASSES_H

#include <pesto/PolyProgram.hpp>
#include <pesto/PolyhedralPass.hpp>
#include <pesto/ScopNest.hpp>
#include <string>
#include <vector>

namespace pesto {

class CloogCodegenPass : public IPolySingleNestPass {
  public:
	CloogCodegenPass(std::string prefix);
	virtual ~CloogCodegenPass();

	virtual std::string getUUID();
	virtual void runSingleNest(ScopNest &nest);

  private:
	std::string path_prefix;
};

} // namespace pesto

#endif // PESTO_CODEGEN_PASSES_H