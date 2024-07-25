#ifndef PESTO_CLAST_PASS_H
#define PESTO_CLAST_PASS_H

#include <pesto/ClastProgram.hpp>
#include <string>

namespace pesto {

class IClastPass {
  public:
	virtual ~IClastPass();

	virtual void run(ClastProgram &prog) = 0;
	virtual std::string getUUID() = 0;
};

class IClastSingleNestPass {
  public:
	virtual ~IClastSingleNestPass();

	virtual void run(ClastProgram &prog);
	virtual std::string getUUID() = 0;

	virtual void runSingleNest(ClastNest &nest) = 0;
};

class ClastParamTilingPass : IClastSingleNestPass {
  public:
	ClastParamTilingPass();
	virtual ~ClastParamTilingPass();

	virtual void run(ClastProgram &prog);
	virtual std::string getUUID();
};

} // namespace pesto

#endif // PESTO_CLAST_PASS_H