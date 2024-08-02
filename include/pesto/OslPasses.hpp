#ifndef PESTO_OSL_PASSES_H
#define PESTO_OSL_PASSES_H

#include <pesto/PolyhedralPass.hpp>

namespace pesto {

class ParamContextPass : public IPolySingleNestPass {
  public:
	ParamContextPass(int contextValue);
	virtual ~ParamContextPass();

	virtual void runSingleNest(ScopNest &nest);
	virtual std::string getUUID();

  private:
	int contextValue;
};

class SplitNestedLoopsPass : public IPolyhedralPass {
  public:
	SplitNestedLoopsPass();
	virtual ~SplitNestedLoopsPass();

	virtual void run(PolyProgram &prog);
	virtual std::string getUUID();
};

class DumpOslPass : public IPolySingleNestPass {
  public:
	DumpOslPass(std::string prefix);
	virtual ~DumpOslPass();

	virtual void runSingleNest(ScopNest &nest);
	virtual std::string getUUID();

  private:
	std::string path_prefix;
};

} // namespace pesto

#endif // PESTO_OSL_PASSES_H