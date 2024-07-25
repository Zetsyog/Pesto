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
	std::unique_ptr<pesto::PolyProgram> polyProgram =
		pesto::PolyProgramBuilder().addScop(scop).build();

	osl_scop_free(scop);

	pesto::OpenScopOptimizer optimizer;

	// register all the polyhedral passes
	// order matter
	optimizer.addPass(new pesto::PlutoAutoTransformPass());
	optimizer.addPass(new pesto::SplitNestedLoopsPass());
	optimizer.addPass(new pesto::PlutoTilingPass());
	optimizer.addPass(new pesto::ParamContextPass(10));

	// define the scop we are working on

	// run the passes
	std::cerr << "[pesto] Running optimizer" << std::endl;
	optimizer.runPasses(*polyProgram.get());
	std::cerr << "[pesto] Done running optimizer" << std::endl;

	std::vector<pesto::ScopNest *> nests = polyProgram->getScopNests();

	// DEBUG
	// TODO: remove
	for (std::vector<pesto::ScopNest *>::iterator it = nests.begin();
		 it != nests.end(); ++it) {
		// dump every nest as a scop file before codegen
		int distance = std::distance(nests.begin(), it);
		pesto::ScopNest *nest = *it;
		std::string output_file_path =
			"output_" + std::to_string(distance) + ".scop";
		FILE *out = fopen(output_file_path.c_str(), "w");
		osl_scop_print(out, nest->getScop());
		fclose(out);
	}

	std::cerr << "[pesto] Building Clast Program" << std::endl;
	std::unique_ptr<pesto::ClastProgram> clastProgram =
		pesto::ClastProgramBuilder().fromPolyProgram(polyProgram.get()).build();

	// clast optimizations
	pesto::ClastOptimizer clastOptimizer;
	clastOptimizer.addPass(new pesto::ClastParamTilingPass());

	clastOptimizer.run(*clastProgram.get());

	std::cerr << "[pesto] Generating code" << std::endl;
	pesto::ClastCodegen codegen;
	codegen.generateCode(*clastProgram.get(), "output_");

	std::cerr << "[pesto] Done" << std::endl;
	return 0;
}
