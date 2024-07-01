#ifndef ATTILA_POLY_PROGRAM_H
#define ATTILA_POLY_PROGRAM_H

#include <attila/ScopNest.hpp>
#include <osl/osl.h>
#include <vector>

class PolyProgram {
  public:
	PolyProgram();
	virtual ~PolyProgram();

	void addScopNest(ScopNest nest);
	void clearScopNests();
	std::vector<ScopNest> getScopNests();

  private:
	std::vector<ScopNest> nests;
};

class PolyProgramBuilder {
  public:
	PolyProgramBuilder();
	virtual ~PolyProgramBuilder();

	PolyProgram *build();

	void addScop(osl_scop_p scop);

	void reset();

  private:
	PolyProgram *program;
};

#endif // ATTILA_POLY_PROGRAM_H