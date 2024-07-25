#ifndef PESTO_CLAST_OPTIMIZER_H
#define PESTO_CLAST_OPTIMIZER_H

#include <memory>
#include <vector>

#include <pesto/ClastPass.hpp>
#include <pesto/ClastProgram.hpp>

namespace pesto {

class ClastPassManager {
  public:
	ClastPassManager();
	virtual ~ClastPassManager();

	void addPass(std::unique_ptr<IClastPass> pass);

	void run(ClastProgram &prog);

  private:
	std::vector<std::unique_ptr<IClastPass>> passes;
};

} // namespace pesto

#endif // PESTO_CLAST_OPTIMIZER_H