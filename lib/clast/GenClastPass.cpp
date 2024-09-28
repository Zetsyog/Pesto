#include <pesto/Pesto.hpp>

#include "osl_support.h"

#include "clast/clast_support.h"

using namespace pesto;

GenClastPass::GenClastPass() : IPolySingleNestPass("pass-clast-gen") {}

GenClastPass::~GenClastPass() {}

void GenClastPass::runSingleNest(ScopNest &nest) {
	nest.metadata.set<ClastMetadata>();
	auto data = nest.metadata.get<ClastMetadata>().value();

	osl_scop_p scop = nest.getScop();

	data->cloogState = cloog_state_malloc();
	data->cloogOptions = cloog_options_malloc(data->cloogState);
	data->stmtsNb = osl_statement_number(scop->statement);

	int max_scattering_dim = osl_max_scattering_dim(scop);

	data->cloogOptions->f = max_scattering_dim + 1;
	data->cloogOptions->l = max_scattering_dim + 1;
	// data.cloogOptions->override = 1;

	data->cloogOptions->compilable = 0;
	data->cloogOptions->esp = 1;
	data->cloogOptions->strides = 0;
	data->cloogOptions->nosimplify = 0;
	data->cloogOptions->save_domains = 1;
	// data.cloogOptions->sh = 1;
	data->cloogOptions->openscop = 1;
	data->cloogOptions->backtrack = 0;

	CloogInput *input = cloog_input_from_osl_scop(data->cloogState, scop);
	// input is freed by cloog_clast_create_from_input
	CloogProgram *program;

	program =
		cloog_program_alloc(input->context, input->ud, data->cloogOptions);
	free(input);

	program = cloog_program_generate(program, data->cloogOptions);

	data->cloogProgram = program;

	data->root = cloog_clast_create(program, data->cloogOptions);
	clast_annotate_loops(scop, data->root);
}

ClastMetadata::ClastMetadata()
	: root(nullptr), cloogProgram(nullptr), cloogOptions(nullptr),
	  cloogState(nullptr) {}

ClastMetadata::~ClastMetadata() {
	if (root) {
		cloog_clast_free(root);
	}
	if (cloogProgram) {
		cloog_program_free(cloogProgram);
	}
	if (cloogOptions) {
		cloog_options_free(cloogOptions);
	}
	if (cloogState) {
		cloog_state_free(cloogState);
	}
}