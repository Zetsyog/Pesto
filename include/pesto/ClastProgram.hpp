#ifndef PESTO_CLAST_PROGRAM_H
#define PESTO_CLAST_PROGRAM_H

#include <cloog/cloog.h>
#include <pesto/ClastNest.hpp>
#include <pesto/PolyProgram.hpp>

class ClastProgram {
  public:
	ClastProgram();
	virtual ~ClastProgram();

	void addNest(ClastNest *nest);

	std::vector<ClastNest *> getNests();

  private:
	std::vector<ClastNest *> nests;
};

class ClastProgramBuilder {
  public:
	ClastProgramBuilder();
	virtual ~ClastProgramBuilder();

	ClastProgram *build();

	ClastProgramBuilder &addNest(ScopNest &nest);
	ClastProgramBuilder &fromPolyProgram(PolyProgram *program);

	void reset();

  private:
	ClastProgram *program;
};

#endif // PESTO_CLAST_PROGRAM_H