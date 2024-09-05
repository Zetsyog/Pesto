#include <iostream>

#include <pesto/Pesto.hpp>

using namespace pesto;

CloogCodegenPass::CloogCodegenPass(std::string prefix)
	: path_prefix(prefix), IPolySingleNestPass("pass-gen-cloog") {}

CloogCodegenPass::~CloogCodegenPass() {}

void CloogCodegenPass::run(PolyProgram &prog) {
	log(std::cerr) << "Starting Cloog codegen using prefix '" << path_prefix
				   << "'" << std::endl;
	IPolySingleNestPass::run(prog);

	// generate var declarations
	VarDeclMetadata varDecls;
	for (auto &nest : prog.getScopNests()) {
		auto varDeclsData = nest->metadata.get<VarDeclMetadata>();
		if (varDeclsData) {
			std::vector<std::string> vars = varDeclsData.value()->getVars();
			for (auto &var : vars) {
				varDecls.addVar(var);
			}
		}
	}
	FILE *vardecl_fd = fopen(".vardecls", "w");
	for (auto &var : varDecls.getVars()) {
		fprintf(vardecl_fd, "%s ", varDecls.type.c_str());
		fprintf(vardecl_fd, "%s;\n", var.c_str());
	}
	fclose(vardecl_fd);
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
	CloogProgram *program = nullptr;

	auto clastData = nest.metadata.get<ClastMetadata>();
	auto varDeclsData = nest.metadata.getOrCreate<VarDeclMetadata>();

	std::string path =
		this->path_prefix + "." + std::to_string(nest.getId()) + ".c";

	if (clastData) {
		log(std::cerr) << "Using existing CLooG AST" << std::endl;
		root = clastData.value()->root;
		state = clastData.value()->cloogState;
		options = clastData.value()->cloogOptions;
		program = clastData.value()->cloogProgram;
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
		program = cloog_program_alloc(input->context, input->ud, options);
		free(input);

		program = cloog_program_generate(program, options);
		// input is freed by cloog_clast_create_from_input
		root = cloog_clast_create(program, options);
	}

	if (root == nullptr || state == nullptr || options == nullptr) {
		log(std::cerr) << "Failed to generate CLooG AST" << std::endl;
		return;
	}

	// Add iterators to var decls
	for (int i = 0; i < program->names->nb_scattering; i += 1) {
		varDeclsData->addVar(program->names->scattering[i]);
	}

	log(std::cerr) << "Writing code to " << path << std::endl;
	FILE *file = fopen(path.c_str(), "w");
	// generate stmt macros
	generateStmtMacro(file, nest);

	// print custom macro if there are any
	auto macrosData = nest.metadata.get<MacroMetadata>();
	if (macrosData) {
		std::vector<std::string> macros = macrosData.value()->list;
		for (auto &macro : macros) {
			fprintf(file, "%s\n\n", macro.c_str());
		}
	}

	// then print cloog ast
	clast_pprint(file, root, 0, options);
	fclose(file);

	if (!clastData) {
		cloog_clast_free(root);
		cloog_program_free(program);
		cloog_options_free(options);
		cloog_state_free(state);
	}
}
