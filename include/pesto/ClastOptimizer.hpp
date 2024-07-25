#ifndef PESTO_CLAST_OPTIMIZER_H
#define PESTO_CLAST_OPTIMIZER_H

#include <pesto/ClastPass.hpp>
#include <pesto/ClastProgram.hpp>
#include <vector>

namespace pesto {

class ClastOptimizer {
  public:
	ClastOptimizer();
	virtual ~ClastOptimizer();

	void addPass(IClastPass *pass);

	void run(ClastProgram &prog);

  private:
	std::vector<IClastPass *> passes;
};

} // namespace pesto

#endif // PESTO_CLAST_OPTIMIZER_H