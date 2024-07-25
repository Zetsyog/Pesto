#include <cloog/cloog.h>
#include <iostream>
#include <pesto/ClastCodegen.hpp>
#include <pesto/ClastNest.hpp>

using namespace pesto;

ClastCodegen::ClastCodegen() {}

ClastCodegen::~ClastCodegen() {}

void ClastCodegen::generateCode(ClastProgram &program,
								std::string const &output) {

	std::cerr << "[pesto-codegen] Starting code generation " << std::endl;

	std::vector<ClastNest *> nests = program.getNests();

	std::cerr << "[pesto-codegen] Found " << nests.size() << " nests"
			  << std::endl;

	for (auto it = nests.begin(); it != nests.end(); ++it) {
		ClastNest *nest = *it;
		int index = std::distance(nests.begin(), it);
		// Your code here

		std::string path = output + std::to_string(index) + ".c";
		FILE *file = fopen(path.c_str(), "w");

		clast_pprint(file, nest->getClastRoot(), 0, nest->getCloogOptions());

		fclose(file);
	}
}