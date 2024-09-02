#ifndef PESTO_POLY_PROGRAM_H
#define PESTO_POLY_PROGRAM_H

#include <osl/osl.h>

#include <pesto/ScopNest.hpp>

#include <any>
#include <map>
#include <memory>
#include <vector>

namespace pesto {

class PolyProgram {
  public:
	PolyProgram();
	virtual ~PolyProgram();

	void addScopNest(ScopNest *nest);
	void clearScopNests();
	std::vector<ScopNest *> getScopNests();

  private:
	std::vector<ScopNest *> nests;
};

class PolyProgramBuilder {
  public:
	PolyProgramBuilder();
	virtual ~PolyProgramBuilder();

	std::unique_ptr<PolyProgram> build();

	PolyProgramBuilder &addScop(osl_scop_p scop);

	void reset();

  private:
	PolyProgram *program;
	int nextNestId;
};

} // namespace pesto

#endif // PESTO_POLY_PROGRAM_H