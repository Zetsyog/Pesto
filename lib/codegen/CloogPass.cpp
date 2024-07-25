#include <iostream>

#include <pesto/Pesto.hpp>

using namespace pesto;

CloogCodegenPass::CloogCodegenPass(std::string prefix) : path_prefix(prefix) {}

CloogCodegenPass::~CloogCodegenPass() {}

std::string CloogCodegenPass::getUUID() { return "codegen-cloog"; }

void CloogCodegenPass::runSingleNest(ScopNest &nest) {
	log(std::cerr) << "Running cloog on nest " << nest.getId() << std::endl;

	struct clast_stmt *root = nullptr;
	CloogState *state = nullptr;
	CloogOptions *options = nullptr;

	std::string path = this->path_prefix + std::to_string(nest.getId()) + ".c";

	if (nest.metadata.has("clast")) {
		ClastMetadata data = nest.metadata.get<ClastMetadata>("clast");
		root = data.root;
		state = data.cloogState;
		options = data.cloogOptions;
	} else {
		osl_scop_p scop = nest.getScop();

		state = cloog_state_malloc();
		options = cloog_options_malloc(state);

		options->f = -1;
		options->l = 1;
		options->compilable = 0;
		options->esp = 1;
		options->strides = 0;
		options->sh = 1;
		options->openscop = 1;
		options->backtrack = 1;

		CloogInput *input = cloog_input_from_osl_scop(state, scop);
		// input is freed by cloog_clast_create_from_input
		struct clast_stmt *root = cloog_clast_create_from_input(input, options);
	}

	if (root == nullptr || state == nullptr || options == nullptr) {
		log(std::cerr) << "Failed to generate CLooG AST" << std::endl;
		return;
	}

	log(std::cerr) << "Writing CLooG AST to " << path << std::endl;
	FILE *file = fopen(path.c_str(), "w");
	clast_pprint(file, root, 0, options);
	fclose(file);

	if (nest.metadata.has("clast")) {
		cloog_clast_free(root);
		cloog_options_free(options);
		cloog_state_free(state);
	}
}
