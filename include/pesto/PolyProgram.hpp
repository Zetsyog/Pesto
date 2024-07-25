#ifndef PESTO_POLY_PROGRAM_H
#define PESTO_POLY_PROGRAM_H

#include <any>
#include <map>
#include <osl/osl.h>
#include <pesto/ScopNest.hpp>
#include <vector>

namespace pesto {

class PolyProgram {
  public:
	PolyProgram();
	virtual ~PolyProgram();

	void addScopNest(ScopNest *nest);
	void clearScopNests();
	std::vector<ScopNest *> getScopNests();
	std::any getMetadata(std::string key);
	void setMetadata(std::string key, std::any value);

  private:
	std::vector<ScopNest *> nests;
	std::map<std::string, std::any> metadata;
};

class PolyProgramBuilder {
  public:
	PolyProgramBuilder();
	virtual ~PolyProgramBuilder();

	PolyProgram *build();

	PolyProgramBuilder &addScop(osl_scop_p scop);

	void reset();

  private:
	PolyProgram *program;
};

} // namespace pesto

#endif // PESTO_POLY_PROGRAM_H