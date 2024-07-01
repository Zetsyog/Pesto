#ifndef ATTILA_CLAST_PROGRAM_H
#define ATTILA_CLAST_PROGRAM_H

#include <attila/ClastNest.hpp>
#include <attila/PolyProgram.hpp>
#include <cloog/cloog.h>

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

	void addNest(ScopNest &nest);
	void fromPolyProgram(PolyProgram *program);

	void reset();

  private:
	ClastProgram *program;
};

#endif // ATTILA_CLAST_PROGRAM_H