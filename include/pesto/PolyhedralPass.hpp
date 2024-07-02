#ifndef PESTO_POLYHEDRAL_PASS_H
#define PESTO_POLYHEDRAL_PASS_H

#include <pesto/PolyProgram.hpp>
#include <pesto/ScopNest.hpp>
#include <string>
#include <vector>

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

class PlutoAutoTransformPass : public IPolySingleNestPass {
  public:
	PlutoAutoTransformPass();
	virtual ~PlutoAutoTransformPass();

	virtual void runSingleNest(ScopNest &nest);
	virtual std::string getUUID();
};

class SplitNestedLoopsPass : public IPolyhedralPass {
  public:
	SplitNestedLoopsPass();
	virtual ~SplitNestedLoopsPass();

	virtual void run(PolyProgram &prog);
	virtual std::string getUUID();
};

#endif // PESTO_POLYHEDRAL_PASS_H