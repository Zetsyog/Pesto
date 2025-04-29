#include <fstream>
#include <iostream>

#include <pesto/CodegenPasses.hpp>
#include <pesto/Pesto.hpp>
#include <pesto/clast/GenClastPass.hpp>
#include <pesto/clast/clast_support.h>

using namespace pesto::pass;

CloogCodegenPass::CloogCodegenPass() : IPass("pass-gen-cloog"), output("") {}

CloogCodegenPass::~CloogCodegenPass() {}

void CloogCodegenPass::hookBegin(Program &prog) {
	using metadata::VarDeclMetadata;

	this->output = prog.getOutputFile();

	log(std::cerr) << "Starting Cloog codegen. Writing in '" << output.string()
				   << "'" << std::endl;

	GenClastPass genClast = GenClastPass();
	genClast.apply(prog);

	// Check that all scops has a ClastHolder metadata
	for (auto scop : prog.getScopNests()) {
		if (!scop->metadata.has<metadata::ClastHolder>()) {
			log(std::cerr) << "Could not generate Cloog AST for nest "
						   << scop->getId() << std::endl;

			return;
		}
	}

	// Generate variable declarations
	VarDeclMetadata varDecls = this->mergeVarDecls(prog);

	log(std::cerr) << "Writing var decls to " << this->output << std::endl;
	std::ofstream outFile;
	outFile.open(this->output);
	if (!outFile.is_open()) {
		log(std::cerr) << "Failed to open file: " << this->output.string()
					   << std::endl;
		return;
	}
	outFile << "/* Variable declarations */" << std::endl;
	// TODO: group variable declaration
	for (auto &varId : varDecls.getVars()) {
		outFile << varDecls.type << " " << varId << ";" << std::endl;
	}

	outFile.close();
}

void generateStmtMacro(FILE *fd, pesto::OslScop &nest) {
	osl_scop_p scop = nest.c_scop();

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

void CloogCodegenPass::applyScop(OslScop &nest) {
	using metadata::ClastHolder;
	using metadata::MacroMetadata;
	using metadata::VarDeclMetadata;

	struct clast_stmt *root = nullptr;
	CloogState *state = nullptr;
	CloogOptions *options = nullptr;
	CloogProgram *program = nullptr;

	auto clastData = nest.metadata.get<ClastHolder>().lock();
	auto varDeclsData = nest.metadata.getOrCreate<VarDeclMetadata>().lock();

	std::string path = this->output.string();

	if (!clastData) {
		log(std::cerr) << "An error occured, ClastData was not generated...";
		return;
	}

	// TODO: allow user to customize cloog options
	root = clastData->root;
	state = clastData->cloogState;
	options = clastData->cloogOptions;
	program = clastData->cloogProgram;

	if (root == nullptr || state == nullptr || options == nullptr ||
		program == nullptr) {
		log(std::cerr) << "Error during Cloog AST generation" << std::endl;
		return;
	}

	log(std::cerr) << "Writing code to " << path << std::endl;
	FILE *file = fopen(path.c_str(), "a");
	// generate stmt macros
	generateStmtMacro(file, nest);

	// print custom macro if there are any
	auto macrosData = nest.metadata.get<MacroMetadata>().lock();
	if (macrosData) {
		std::vector<std::string> macros = macrosData->list;
		for (auto &macro : macros) {
			fprintf(file, "%s\n\n", macro.c_str());
		}
	}

	// then print cloog ast
	clast_pprint(file, root, 0, options);
	fclose(file);
}

pesto::pass::metadata::VarDeclMetadata
CloogCodegenPass::mergeVarDecls(Program &prog) {
	using pesto::pass::metadata::ClastHolder;
	using pesto::pass::metadata::VarDeclMetadata;

	VarDeclMetadata data;
	// Cloog specific (var used for parallel or vectorized for)
	data.addVar("lbp");
	data.addVar("ubp");
	data.addVar("lbv");
	data.addVar("ubv");

	// merge existing var decls of all scops
	for (auto &scop : prog.getScopNests()) {
		// add existing var decls
		auto varDeclsData = scop->metadata.get<VarDeclMetadata>().lock();
		if (varDeclsData) {
			std::vector<std::string> vars = varDeclsData->getVars();
			for (auto &var : vars) {
				data.addVar(var);
			}
		}
	}

	return data;
}
