#ifndef ATTILA_POLYHEDRAL_PASS_H
#define ATTILA_POLYHEDRAL_PASS_H

#include <attila/PolyProgram.hpp>
#include <attila/ScopNest.hpp>
#include <string>
#include <vector>

class IPolyhedralPass {
  public:
	virtual void run(PolyProgram &prog) = 0;
	virtual ~IPolyhedralPass();
	virtual std::string getUUID() = 0;
};

class IPolySingleNestPass : public IPolyhedralPass {
  public:
	virtual void runSingleNest(ScopNest &nest) = 0;
	virtual void run(PolyProgram &prog);
	virtual ~IPolySingleNestPass();
	virtual std::string getUUID() = 0;
};

class PlutoAutoTransformPass : public IPolySingleNestPass {
  public:
	PlutoAutoTransformPass();
	virtual ~PlutoAutoTransformPass();
	virtual void runSingleNest(ScopNest &nest);
	virtual std::string getUUID();
};

#endif // ATTILA_POLYHEDRAL_PASS_H