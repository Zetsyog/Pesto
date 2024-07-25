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

class IClastSingleNestPass : public IClastPass {
  public:
	virtual ~IClastSingleNestPass();

	virtual void run(ClastProgram &prog);

	virtual std::string getUUID() = 0;
	virtual void runSingleNest(ClastNest &nest) = 0;
};

class ClastParamTilingPass : public IClastSingleNestPass {
  public:
	ClastParamTilingPass();
	virtual ~ClastParamTilingPass();

	virtual std::string getUUID();
	virtual void runSingleNest(ClastNest &nest);
};

} // namespace pesto

#endif // PESTO_CLAST_PASS_H