#ifndef PESTO_CLAST_CODEGEN_H
#define PESTO_CLAST_CODEGEN_H

#include <pesto/ClastProgram.hpp>
#include <string>

class ClastCodegen {
  public:
	ClastCodegen();
	virtual ~ClastCodegen();

	void generateCode(ClastProgram &program, std::string const &output);
};

#endif