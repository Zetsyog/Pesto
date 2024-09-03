#ifndef PESTO_POLYHEDRAL_PASS_H
#define PESTO_POLYHEDRAL_PASS_H

#include <iostream>
#include <pesto/PolyProgram.hpp>
#include <pesto/ScopNest.hpp>
#include <string>
#include <vector>

#include <pluto/pluto.h>

namespace pesto {

/**
 * Base interface for a pass that operates on a PolyProgram
 * A pass must have a name
 * A pass must implement the run method that will be called on the PolyProgram
 * by the PassManager
 */
class IPolyhedralPass {
  public:
	IPolyhedralPass(std::string name);
	virtual ~IPolyhedralPass();

	virtual void run(PolyProgram &prog) = 0;

	virtual std::string getName() { return name; }

  protected:
	std::string name;
	virtual std::ostream &log(std::ostream &os);
};

class IPolySingleNestPass : public IPolyhedralPass {
  public:
	IPolySingleNestPass(std::string name);
	virtual ~IPolySingleNestPass();

	virtual void runSingleNest(ScopNest &nest) = 0;
	virtual void run(PolyProgram &prog);
};

} // namespace pesto

#endif // PESTO_POLYHEDRAL_PASS_H