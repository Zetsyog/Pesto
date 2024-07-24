#ifndef PESTO_CLAST_PASS_H
#define PESTO_CLAST_PASS_H

#include <pesto/ClastProgram.hpp>
#include <string>

class IClastPass {
  public:
	virtual ~IClastPass();

	virtual void run(ClastProgram &prog) = 0;
	virtual std::string getUUID() = 0;
};

class ClastParamTilingPass : IClastPass {
  public:
	ClastParamTilingPass();
	virtual ~ClastParamTilingPass();

	virtual void run(ClastProgram &prog);
	virtual std::string getUUID();
};

#endif // PESTO_CLAST_PASS_H