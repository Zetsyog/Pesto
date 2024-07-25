#include <pesto/Pesto.hpp>

#include "osl_support.h"

using namespace pesto;

GenClastPass::GenClastPass() {}

GenClastPass::~GenClastPass() {}

std::string GenClastPass::getUUID() { return "clast-gen"; }

void GenClastPass::runSingleNest(ScopNest &nest) {
	osl_scop_p scop = nest.getScop();

	ClastMetadata data;
	data.cloogState = cloog_state_malloc();
	data.cloogOptions = cloog_options_malloc(data.cloogState);

	std::cerr << osl_max_scattering_dim(scop) << std::endl;

	data.cloogOptions->f = osl_max_scattering_dim(scop) + 1;
	data.cloogOptions->l = osl_max_scattering_dim(scop) + 1;
	data.cloogOptions->compilable = 0;
	data.cloogOptions->esp = 1;
	data.cloogOptions->strides = 0;
	// data.cloogOptions->sh = 1;
	data.cloogOptions->openscop = 1;
	data.cloogOptions->backtrack = 0;

	CloogInput *input = cloog_input_from_osl_scop(data.cloogState, scop);
	// input is freed by cloog_clast_create_from_input
	struct clast_stmt *root =
		cloog_clast_create_from_input(input, data.cloogOptions);

	data.root = root;

	nest.metadata.set("clast", data);
}