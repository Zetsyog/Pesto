#ifndef ATTILA_OPENSCOP_OPTIMIZER_H
#define ATTILA_OPENSCOP_OPTIMIZER_H

#include <attila/PolyProgram.hpp>
#include <attila/PolyhedralPass.hpp>
#include <attila/ScopNest.hpp>
#include <osl/osl.h>
#include <vector>

/**
 * @brief Openscop optimizer class
 * Used to run different optimization passes on a polyhedral code
 * Take as input a polyhedral program in OpenScop format
 * Two kinds of passes can be run:
 * - Polyhedral passes
 * - AST passes
 * Then c code can be generated
 *
 */
class OpenScopOptimizer {
  public:
	OpenScopOptimizer();
	virtual ~OpenScopOptimizer();

	/**
	 * Run optimization passes
	 * Multiple passes can be run
	 * TODO: wip
	 */
	void runPasses(PolyProgram &prog);

	void addPass(IPolyhedralPass *pass);

  private:
	std::vector<IPolyhedralPass *> passes;
};

#endif // ATTILA_OPENSCOP_OPTIMIZER_H