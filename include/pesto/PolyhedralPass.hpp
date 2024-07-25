#ifndef PESTO_POLYHEDRAL_PASS_H
#define PESTO_POLYHEDRAL_PASS_H

#include <iostream>
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

  protected:
	std::ostream &log(std::ostream &os);
};

class IPolySingleNestPass : public IPolyhedralPass {
  public:
	virtual ~IPolySingleNestPass();

	virtual void runSingleNest(ScopNest &nest) = 0;
	virtual void run(PolyProgram &prog);
	virtual std::string getUUID() = 0;
};

} // namespace pesto

#endif // PESTO_POLYHEDRAL_PASS_H