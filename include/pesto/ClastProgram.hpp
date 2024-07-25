#ifndef PESTO_CLAST_PROGRAM_H
#define PESTO_CLAST_PROGRAM_H

#include <memory>
#include <vector>

#include <cloog/cloog.h>
#include <pesto/ClastNest.hpp>
#include <pesto/PolyProgram.hpp>

namespace pesto {

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

	std::unique_ptr<ClastProgram> build();

	ClastProgramBuilder &addNest(ScopNest *nest);
	ClastProgramBuilder &fromPolyProgram(PolyProgram *program);

	void reset();

  private:
	ClastProgram *program;
};

} // namespace pesto

#endif // PESTO_CLAST_PROGRAM_H