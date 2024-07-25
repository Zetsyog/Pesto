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

	// frontend: read openscop file
	// TODO: add support for other formats (c code, isl, etc)
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

	pesto::PassManager pestoPM;

	// register all the polyhedral passes
	// order matter
	pestoPM.addPass(std::make_unique<pesto::ParamContextPass>(10));
	pestoPM.addPass(std::make_unique<pesto::PlutoAutoTransformPass>());
	pestoPM.addPass(std::make_unique<pesto::SplitNestedLoopsPass>());
	pestoPM.addPass(std::make_unique<pesto::PlutoTilingPass>());
	pestoPM.addPass(std::make_unique<pesto::ClastGenPass>());
	pestoPM.addPass(std::make_unique<pesto::ClastParamTilingPass>());
	pestoPM.addPass(std::make_unique<pesto::CloogCodegenPass>("output."));

	// define the scop we are working on

	// run the passes
	std::cerr << "[pesto] Running optimizations passes" << std::endl;
	pestoPM.runPasses(*polyProgram.get());
	std::cerr << "[pesto] Done passes" << std::endl;

	std::cerr << "[pesto] Done" << std::endl;
	return 0;
}
