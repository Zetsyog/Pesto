#ifndef PESTO_OPENSCOP_OPTIMIZER_H
#define PESTO_OPENSCOP_OPTIMIZER_H

#include <memory>
#include <osl/osl.h>
#include <pesto/PolyProgram.hpp>
#include <pesto/PolyhedralPass.hpp>
#include <pesto/ScopNest.hpp>
#include <vector>

namespace pesto {

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
class OslPassManager {
  public:
	OslPassManager();
	virtual ~OslPassManager();

	/**
	 * Run optimization passes
	 * Multiple passes can be run
	 * TODO: wip
	 */
	void runPasses(PolyProgram &prog);

	void addPass(std::unique_ptr<IPolyhedralPass> pass);

  private:
	std::vector<std::unique_ptr<IPolyhedralPass>> passes;
};

} // namespace pesto

#endif // PESTO_OPENSCOP_OPTIMIZER_H