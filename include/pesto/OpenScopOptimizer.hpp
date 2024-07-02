#ifndef PESTO_OPENSCOP_OPTIMIZER_H
#define PESTO_OPENSCOP_OPTIMIZER_H

#include <osl/osl.h>
#include <pesto/PolyProgram.hpp>
#include <pesto/PolyhedralPass.hpp>
#include <pesto/ScopNest.hpp>
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

#endif // PESTO_OPENSCOP_OPTIMIZER_H