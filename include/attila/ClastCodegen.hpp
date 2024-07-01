#ifndef ATTILA_CLAST_CODEGEN_H
#define ATTILA_CLAST_CODEGEN_H

#include <attila/ClastProgram.hpp>
#include <string>

class ClastCodegen {
  public:
	ClastCodegen();
	virtual ~ClastCodegen();

	void generateCode(ClastProgram &program, std::string const &output);
};

#endif