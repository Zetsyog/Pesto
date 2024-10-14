#ifndef PESTO_OSL_PASSES_H
#define PESTO_OSL_PASSES_H

#include <pesto/Pass.hpp>

#include <map>

namespace pesto {

/**
 * A pass that adds a minimal value to global parameters of each scop of the
 * program
 */
class ParamContextPass : public IPestoSingleNestPass {
  public:
	ParamContextPass(int contextValue);
	virtual ~ParamContextPass();

	virtual void runSingleNest(ScopNest &nest);

  private:
	int contextValue;
};

class OpenScopMetadata {
  public:
	OpenScopMetadata();
	virtual ~OpenScopMetadata();

	std::map<std::string, osl_scop_p> scops;
};

/**
 * A pass that splits nested loops in each scop of the program
 */
class SplitNestedLoopsPass : public IPestoPass {
  public:
	SplitNestedLoopsPass();
	virtual ~SplitNestedLoopsPass();

	virtual void run(PestoProgram &prog);
};

/**
 * A pass that dumps the osl representation of each scop of the program
 * Useful for debugging
 */
class DumpOslPass : public IPestoSingleNestPass {
  public:
	DumpOslPass(std::string prefix);
	virtual ~DumpOslPass();

	virtual void runSingleNest(ScopNest &nest);

  private:
	std::string path_prefix;
};

} // namespace pesto

#endif // PESTO_OSL_PASSES_H