#ifndef PESTO_POLY_PROGRAM_H
#define PESTO_POLY_PROGRAM_H

#include <osl/osl.h>

#include <pesto/ScopNest.hpp>

#include <any>
#include <map>
#include <memory>
#include <vector>

namespace pesto {

/**
 * A class representing a polyhedral program
 * A polyhedral program is a collection of ScopNest objects
 * ScopNest objects can be added and / or removed from the program
 * The order of the ScopNest objects does matter
 */
class PestoProgram {
  public:
	PestoProgram();
	virtual ~PestoProgram();

	void addScopNest(ScopNest *nest);
	void clearScopNests();
	std::vector<ScopNest *> getScopNests();

  private:
	std::vector<ScopNest *> nests;
};

/**
 * This class is used to build a PestoProgram object from an OpenScop
 * representation
 */
class PestoProgramBuilder {
  public:
	PestoProgramBuilder();
	virtual ~PestoProgramBuilder();

	std::unique_ptr<PestoProgram> build();

	PestoProgramBuilder &addScop(osl_scop_p scop);

	void reset();

  private:
	PestoProgram *program;
	int nextNestId;
};

} // namespace pesto

#endif // PESTO_POLY_PROGRAM_H