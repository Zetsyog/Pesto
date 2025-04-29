#ifndef PESTO_POLYHEDRAL_PASS_H
#define PESTO_POLYHEDRAL_PASS_H

#include <iostream>
#include <string>
#include <vector>

#include <pluto/pluto.h>

#include <pesto/OslScop.hpp>
#include <pesto/Program.hpp>

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

	/**
	 * @brief apply the pass on the program
	 * @param prog the program to apply the pass on
	 */
	void apply(pesto::Program &prog);

	/**
	 * @brief hook if some computation must be done on the entire program
	 * Called before the applyScop method
	 * @param prog the program to apply the pass on
	 */
	virtual void hookBegin(pesto::Program &prog);

	/**
	 * @brief hook if some computation must be done on the entire program
	 * Called after the applyScop method
	 * @param prog the program to apply the pass on
	 */
	virtual void hookEnd(pesto::Program &prog);

	/**
	 * @brief apply the pass on a single scop
	 * @param nest the nest to apply the pass on
	 */
	virtual void applyScop(pesto::OslScop &nest);

	virtual std::string getName() { return name; }

  protected:
	std::string name;
	virtual std::ostream &log(std::ostream &os);
};

} // namespace pass

} // namespace pesto

#endif // PESTO_POLYHEDRAL_PASS_H