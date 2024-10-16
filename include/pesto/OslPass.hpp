#ifndef PESTO_PASS_OSL_H
#define PESTO_PASS_OSL_H

#include <pesto/Pass.hpp>

#include <map>

namespace pesto {

namespace pass {

namespace metadata {

class OpenScopMetadata {
  public:
	OpenScopMetadata();
	virtual ~OpenScopMetadata();

	std::map<std::string, osl_scop_p> scops;
};
} // namespace metadata

/**
 * A pass that adds a minimal value to global parameters of each scop of the
 * program
 */
class ParamContextPass : public ISinglePass {
  public:
	ParamContextPass(int contextValue);
	virtual ~ParamContextPass();

	virtual void runSingleNest(OslScop &nest);

  private:
	int contextValue;
};

/**
 * A pass that splits nested loops in each scop of the program
 */
class SplitNestedLoopsPass : public IPass {
  public:
	SplitNestedLoopsPass();
	virtual ~SplitNestedLoopsPass();

	virtual void run(Program &prog);
};

/**
 * A pass that dumps the osl representation of each scop of the program
 * Useful for debugging
 */
class DumpOslPass : public ISinglePass {
  public:
	DumpOslPass(std::string prefix);
	virtual ~DumpOslPass();

	virtual void runSingleNest(OslScop &nest);

  private:
	std::string path_prefix;
};

} // namespace pass

} // namespace pesto

#endif // PESTO_PASS_OSL_H