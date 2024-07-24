#include <iostream>
#include <osl/osl.h>
#include <pesto/Pesto.hpp>
#include <stdio.h>

void print_usage() {
	std::cerr << "Usage: pesto [options] <input file>" << std::endl;
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
	std::cerr << "[pesto] Parsing file " << argv[1] << std::endl;
	FILE *in = fopen(input_file_path.c_str(), "r");
	if (in == NULL) {
		std::cerr << "[pesto] Error while opening input file" << std::endl;
		return 1;
	}
	osl_scop_p scop = osl_scop_read(in);
	fclose(in);

	if (scop == NULL) {
		std::cerr << "[pesto] Error parsing scop file" << std::endl;
		return 1;
	}

	// create Polyhedral Program
	PolyProgram *polyProgram = PolyProgramBuilder().addScop(scop).build();
	osl_scop_free(scop);

	OpenScopOptimizer optimizer;

	// register passes
	PlutoAutoTransformPass *auto_transform_pass = new PlutoAutoTransformPass();
	SplitNestedLoopsPass *split_nested_loops_pass = new SplitNestedLoopsPass();
	PlutoTilingPass *tiling_pass = new PlutoTilingPass();

	// order matter
	optimizer.addPass(auto_transform_pass);
	optimizer.addPass(split_nested_loops_pass);
	optimizer.addPass(tiling_pass);

	// define the scop we are working on

	// run the passes
	std::cerr << "[pesto] Running optimizer" << std::endl;
	optimizer.runPasses(*polyProgram);
	std::cerr << "[pesto] Done running optimizer" << std::endl;

	std::vector<ScopNest *> nests = polyProgram->getScopNests();

	for (std::vector<ScopNest *>::iterator it = nests.begin();
		 it != nests.end(); ++it) {
		int distance = std::distance(nests.begin(), it);
		ScopNest *nest = *it;
		std::string output_file_path =
			"output_" + std::to_string(distance) + ".scop";
		FILE *out = fopen(output_file_path.c_str(), "w");
		osl_scop_print(out, nest->getScop());
		fclose(out);
	}

	std::cout << "Number of iterations: " << nests.size() << std::endl;

	std::cerr << "[pesto] Building Clast Program" << std::endl;
	ClastProgram *clastProgram =
		ClastProgramBuilder().fromPolyProgram(polyProgram).build();

	delete polyProgram;

	std::cerr << "[pesto] Generating code" << std::endl;
	ClastCodegen codegen;
	codegen.generateCode(*clastProgram, "output_");

	delete clastProgram;

	std::cerr << "[pesto] Done" << std::endl;
	return 0;
}
