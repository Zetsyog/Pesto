#include <attila/ClastCodegen.hpp>
#include <attila/ClastNest.hpp>
#include <cloog/cloog.h>
#include <iostream>

ClastCodegen::ClastCodegen() {}

ClastCodegen::~ClastCodegen() {}

void ClastCodegen::generateCode(ClastProgram &program,
								std::string const &output) {

	std::cerr << "[attila-codegen] Starting code generation " << std::endl;

	std::vector<ClastNest *> nests = program.getNests();

	std::cerr << "[attila-codegen] Found " << nests.size() << " nests"
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