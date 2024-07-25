#ifndef PESTO_POLYHEDRAL_PASS_H
#define PESTO_POLYHEDRAL_PASS_H

#include <pesto/PolyProgram.hpp>
#include <pesto/ScopNest.hpp>
#include <string>
#include <vector>

#include <pluto/pluto.h>

namespace pesto {

class IPolyhedralPass {
  public:
	virtual ~IPolyhedralPass();

	virtual void run(PolyProgram &prog) = 0;
	virtual std::string getUUID() = 0;
};

class IPolySingleNestPass : public IPolyhedralPass {
  public:
	virtual ~IPolySingleNestPass();

	virtual void runSingleNest(ScopNest &nest) = 0;
	virtual void run(PolyProgram &prog);
	virtual std::string getUUID() = 0;
};

class PlutoPass : public IPolySingleNestPass {
  public:
	PlutoPass(std::string suffix);
	virtual ~PlutoPass();

	virtual void runSingleNest(ScopNest &nest);
	virtual std::string getUUID();

	PlutoOptions *getOptions();

  private:
	std::string suffix;
	PlutoContext *context;
};

class PlutoAutoTransformPass : public PlutoPass {
  public:
	PlutoAutoTransformPass();
	virtual ~PlutoAutoTransformPass();
};

class PlutoTilingPass : public PlutoPass {
  public:
	PlutoTilingPass();
	virtual ~PlutoTilingPass();
};

class SplitNestedLoopsPass : public IPolyhedralPass {
  public:
	SplitNestedLoopsPass();
	virtual ~SplitNestedLoopsPass();

	virtual void run(PolyProgram &prog);
	virtual std::string getUUID();
};

} // namespace pesto

#endif // PESTO_POLYHEDRAL_PASS_H