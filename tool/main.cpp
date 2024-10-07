#include <iostream>

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <clan/clan.h>
#include <osl/osl.h>

#include <pesto/Pesto.hpp>

#include "Options.hpp"

int main(int argc, char **argv) {
	auto options = PestoOptions::build(argc, argv);

	/* FRONTEND */
	// check if input file is provided
	std::cerr << "[pesto-tool] Received input file '" << options->getInputPath()
			  << "'" << std::endl;

	// check if input file is regular file, else shout
	struct stat path_stat;
	stat(options->getInputPath().c_str(), &path_stat);
	if (!S_ISREG(path_stat.st_mode)) {
		std::cerr << "[pesto-tool] Input file is not a regular file"
				  << std::endl;
		return 1;
	}

	// open input file
	FILE *in = fopen(options->getInputPath().c_str(), "r");
	if (in == NULL) {
		std::cerr << "[pesto-tool] Error while opening input file '"
				  << options->getInputPath() << "'" << std::endl;
		return 1;
	}

	osl_scop_p scop;

	// if input is scop file
	if (options->getReadScop()) {
		std::cerr << "[pesto-tool] Considering input as openscop file"
				  << std::endl;
		scop = osl_scop_read(in);
		if (scop == NULL) {
			std::cerr << "[pesto-tool] Error parsing scop file" << std::endl;
			return 1;
		}
	} else { // else we consider it is plain c and extract openscop using clan
		std::cerr << "[pesto-tool] Considering input file as plain c"
				  << std::endl;
		std::cerr << "[pesto-tool] Running clan on '" << options->getInputPath()
				  << "'" << std::endl;
		clan_options_p clan_opt = clan_options_malloc();
		scop = clan_scop_extract(in, clan_opt);
		clan_options_free(clan_opt);
		if (scop == NULL) {
			std::cerr << "[pesto-tool] Clan scop extraction failed. Abort"
					  << std::endl;
			return 1;
		}

		if (osl_generic_lookup(scop->extension, OSL_URI_COORDINATES) != NULL) {
			osl_generic_remove(&scop->extension, OSL_URI_COORDINATES);
		}
	}

	fclose(in);
	/* DONE FRONTEND */

	// pesto magic starts here
	// create pesto program
	std::unique_ptr<pesto::PolyProgram> polyProgram =
		pesto::PolyProgramBuilder().addScop(scop).build();
	// scop has been copied by builder, we can free memory
	osl_scop_free(scop);

	std::unique_ptr<pesto::PassManager> pestoPM;

	if (options->getConfigPath().has_value()) {
		std::unique_ptr<pesto::PMBuilder> pmBuilder =
			pesto::PMBuilder::createDefault();

		pestoPM = pmBuilder->build(options->getConfigPath().value());
		if (pestoPM == nullptr) {
			std::cerr << "[pesto-tool] Error while building pass manager"
					  << std::endl;
			return 1;
		}
	} else {
		pestoPM = std::make_unique<pesto::PassManager>();

		// register all the passes
		// order matter
		pestoPM->addPass(std::make_unique<pesto::ParamContextPass>(10));
		pestoPM->addPass(std::make_unique<pesto::PlutoAutoTransformPass>());
		pestoPM->addPass(std::make_unique<pesto::SplitNestedLoopsPass>());
		pestoPM->addPass(std::make_unique<pesto::PlutoTilingPass>());
		pestoPM->addPass(std::make_unique<pesto::DumpOslPass>("debug."));
		pestoPM->addPass(std::make_unique<pesto::GenClastPass>());
		pestoPM->addPass(std::make_unique<pesto::ClastParamTilingPass>());
		// TODO: should codegen be a pass? Or whole different component ?
		pestoPM->addPass(
			std::make_unique<pesto::CloogCodegenPass>(options->getGenPrefix()));
	}

	// run the passes
	std::cerr << "[pesto-tool] Running optimizations passes" << std::endl;
	pestoPM->runPasses(*polyProgram.get());

	std::cerr << "[pesto-tool] Done" << std::endl;
	return 0;
}
