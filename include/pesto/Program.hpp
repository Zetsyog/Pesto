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
	class Builder;

	virtual ~Program();

	void addScopNest(OslScop *nest);
	void clearScopNests();
	std::vector<OslScop *> getScopNests();

  private:
	std::vector<OslScop *> scops;
};

/**
 * This class is used to build a pesto::Program object from an OpenScop
 * representation
 */
class Program::Builder {
  public:
	Builder();

	Builder &addScop(osl_scop_p scop);

	std::unique_ptr<Program> build();

  private:
	int nextScopId;
	std::vector<OslScop *> scops;
};

} // namespace pesto

#endif // PESTO_POLY_PROGRAM_H