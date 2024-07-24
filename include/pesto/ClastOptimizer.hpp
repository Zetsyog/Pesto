#ifndef PESTO_CLAST_OPTIMIZER_H
#define PESTO_CLAST_OPTIMIZER_H

#include <pesto/ClastPass.hpp>
#include <pesto/ClastProgram.hpp>
#include <vector>

class ClastOptimizer {
  public:
	ClastOptimizer();
	virtual ~ClastOptimizer();

	void addPass(IClastPass *pass);

	void run(ClastProgram &prog);

  private:
	std::vector<IClastPass *> passes;
};

#endif // PESTO_CLAST_OPTIMIZER_H