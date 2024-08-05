#include <iostream>

#include <pesto/Pesto.hpp>

using namespace pesto;

CloogCodegenPass::CloogCodegenPass(std::string prefix) : path_prefix(prefix) {}

CloogCodegenPass::~CloogCodegenPass() {}

std::string CloogCodegenPass::getUUID() { return "codegen-cloog"; }

void CloogCodegenPass::run(PolyProgram &prog) {
	log(std::cerr) << "Starting Cloog codegen using prefix '" << path_prefix
				   << "'" << std::endl;
	IPolySingleNestPass::run(prog);
}

void generateStmtMacro(FILE *fd, ScopNest &nest) {
	osl_scop_p scop = nest.getScop();

	osl_statement_p statement = scop->statement;

	int idx = 0;
	while (statement != NULL) {
		osl_body_p body =
			(osl_body_p)osl_generic_lookup(statement->extension, OSL_URI_BODY);
		if (body == NULL) {
			continue;
		}
		int it_size = osl_strings_size(body->iterators);
		// osl statements are 1-indexed
		fprintf(fd, "#define S%d(", idx + 1);
		for (int i = 0; i < it_size; i += 1) {
			if (i != 0) {
				fprintf(fd, ", ");
			}
			fprintf(fd, "%s", body->iterators->string[i]);
		}
		fprintf(fd, ") ");
		osl_strings_print(fd, body->expression);

		idx += 1;
		statement = statement->next;
	}
}

void CloogCodegenPass::runSingleNest(ScopNest &nest) {
	struct clast_stmt *root = nullptr;
	CloogState *state = nullptr;
	CloogOptions *options = nullptr;

	std::string path =
		this->path_prefix + "." + std::to_string(nest.getId()) + ".c";

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

	log(std::cerr) << "Writing code to " << path << std::endl;
	FILE *file = fopen(path.c_str(), "w");
	// generate stmt macros
	generateStmtMacro(file, nest);

	// print custom macro if there are any
	if (nest.metadata.has("macros")) {
		std::vector<std::string> macros =
			nest.metadata.get<std::vector<std::string>>("macros");
		for (auto &macro : macros) {
			fprintf(file, "%s\n\n", macro.c_str());
		}
	}

	// then print cloog ast
	clast_pprint(file, root, 0, options);
	fclose(file);

	if (nest.metadata.has("clast")) {
		ClastMetadata data = nest.metadata.get<ClastMetadata>("clast");

		cloog_clast_free(data.root);
		cloog_program_free(data.cloogProgram);

		cloog_options_free(data.cloogOptions);
		cloog_state_free(data.cloogState);
	}
}
