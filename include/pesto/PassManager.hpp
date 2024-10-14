#ifndef PESTO_OPENSCOP_OPTIMIZER_H
#define PESTO_OPENSCOP_OPTIMIZER_H

#include <memory>
#include <osl/osl.h>
#include <pesto/Pass.hpp>
#include <pesto/Program.hpp>
#include <pesto/ScopNest.hpp>
#include <vector>

namespace pesto {

/**
 * @brief PassManager class
 * Used to run different optimization passes on a polyhedral code
 * Take as input a polyhedral program in OpenScop format
 * Two kinds of passes can be run:
 * - Polyhedral passes
 * - AST passes
 * Then c code can be generated
 *
 */
class PassManager {
  public:
	PassManager();
	virtual ~PassManager();

	/**
	 * Run optimization passes
	 * Multiple passes can be run
	 * TODO: wip
	 */
	void runPasses(PestoProgram &prog);

	void addPass(std::unique_ptr<IPestoPass> pass);

  private:
	std::vector<std::unique_ptr<IPestoPass>> passes;
};

} // namespace pesto

#endif // PESTO_OPENSCOP_OPTIMIZER_H