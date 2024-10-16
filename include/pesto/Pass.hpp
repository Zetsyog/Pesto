#ifndef PESTO_POLYHEDRAL_PASS_H
#define PESTO_POLYHEDRAL_PASS_H

#include <iostream>
#include <pesto/OslScop.hpp>
#include <pesto/Program.hpp>
#include <string>
#include <vector>

#include <pluto/pluto.h>

namespace pesto {

namespace pass {

/**
 * Base interface for a pass that operates on a Program
 * A pass must have a name
 * A pass must implement the run method that will be called on the Program
 * by the PassManager
 */
class IPass {
  public:
	IPass(std::string name);
	virtual ~IPass();

	virtual void run(pesto::Program &prog) = 0;

	virtual std::string getName() { return name; }

  protected:
	std::string name;
	virtual std::ostream &log(std::ostream &os);
};

class ISinglePass : public IPass {
  public:
	ISinglePass(std::string name);
	virtual ~ISinglePass();

	virtual void runSingleNest(pesto::OslScop &nest) = 0;
	virtual void run(pesto::Program &prog);
};

} // namespace pass

} // namespace pesto

#endif // PESTO_POLYHEDRAL_PASS_H