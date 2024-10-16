#ifndef PESTO_POLY_PROGRAM_H
#define PESTO_POLY_PROGRAM_H

#include <osl/osl.h>

#include <pesto/OslScop.hpp>

#include <any>
#include <map>
#include <memory>
#include <vector>

namespace pesto {

/**
 * A class representing a polyhedral program
 * A polyhedral program is a collection of OslScop objects
 * OslScop objects can be added and / or removed from the program
 * The order of the OslScop objects does matter
 */
class Program {
  public:
	Program();
	virtual ~Program();

	void addScopNest(OslScop *nest);
	void clearScopNests();
	std::vector<OslScop *> getScopNests();

  private:
	std::vector<OslScop *> nests;
};

namespace builder {
/**
 * This class is used to build a pesto::Program object from an OpenScop
 * representation
 */
class Program {
  public:
	Program();
	virtual ~Program();

	std::unique_ptr<pesto::Program> build();

	pesto::builder::Program &addScop(osl_scop_p scop);

	void reset();

  private:
	pesto::Program *program;
	int nextNestId;
};

} // namespace builder

} // namespace pesto

#endif // PESTO_POLY_PROGRAM_H