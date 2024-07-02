#include <attila/Attila.hpp>
#include <iostream>
#include <osl/osl.h>
#include <stdio.h>

void print_usage() {
	std::cerr << "Usage: attila [options] <input file>" << std::endl;
	std::cerr << "Options:" << std::endl;
	std::cerr << "  -h, --help\t\tPrint this help message" << std::endl;
	std::cerr << "  -v, --version\t\tPrint the version number" << std::endl;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		print_usage();
		return 1;
	}
	std::string input_file_path = argv[1];
	std::cerr << "[attila] Parsing file " << argv[1] << std::endl;
	FILE *in = fopen(input_file_path.c_str(), "r");
	osl_scop_p scop = osl_scop_read(in);
	fclose(in);

	// create Polyhedral Program
	PolyProgram *polyProgram = PolyProgramBuilder().addScop(scop).build();

	OpenScopOptimizer optimizer;

	// register passes
	PlutoAutoTransformPass *auto_transform_pass = new PlutoAutoTransformPass();
	SplitNestedLoopsPass *split_nested_loops_pass = new SplitNestedLoopsPass();

	// order matter
	optimizer.addPass(auto_transform_pass);
	optimizer.addPass(split_nested_loops_pass);

	// define the scop we are working on

	// run the passes
	std::cerr << "[attila] Running optimizer" << std::endl;
	optimizer.runPasses(*polyProgram);
	std::cerr << "[attila] Done running optimizer" << std::endl;

	std::vector<ScopNest> nests = polyProgram->getScopNests();

	for (auto it = nests.begin(); it != nests.end(); ++it) {
		int iteration = std::distance(nests.begin(), it);
		osl_scop_p scop = it->getScop();

		std::string output_file_path =
			"output_" + std::to_string(iteration) + ".scop";
		FILE *out = fopen(output_file_path.c_str(), "w");
		osl_scop_print(out, scop);
		fclose(out);
	}

	std::cerr << "[attila] Building Clast Program" << std::endl;
	ClastProgram *clastProgram =
		ClastProgramBuilder().fromPolyProgram(polyProgram).build();

	delete polyProgram;

	std::cerr << "[attila] Generating code" << std::endl;
	ClastCodegen codegen;
	codegen.generateCode(*clastProgram, "output_");

	delete clastProgram;

	std::cerr << "[attila] Done" << std::endl;
	return 0;
}
