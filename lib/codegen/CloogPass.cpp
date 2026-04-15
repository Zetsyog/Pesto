/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <fstream>
#include <iostream>

#include <pesto/clast_support.h>
#include <pesto/pass/CodegenPasses.hpp>
#include <pesto/pass/clast/GenClastPass.hpp>

using namespace pesto::pass;

CloogCodegenPass::CloogCodegenPass() : IPass("codegen-cloog"), output(""), genClastPass() {}

CloogCodegenPass::~CloogCodegenPass() {}

void CloogCodegenPass::apply(Program &prog) {
	using metadata::VarDeclMetadata;

	this->output = prog.getOutputFile();

	logger->info("Starting Cloog codegen. Writing in '{}'", output.string().c_str());

	this->genClastPass->apply(prog);

	// Check that all scops has a ClastHolder metadata
	for (auto scop : prog.getScopNests()) {
		if (!scop->metadata.has<metadata::ClastHolder>()) {
			logger->error("Could not generate Cloog AST for nest {}", scop->getId());

			return;
		}
	}

	// Generate variable declarations
	VarDeclMetadata varDecls = this->mergeVarDecls(prog);

	logger->info("Writing var decls to {}", this->output.string().c_str());
	std::ofstream outFile;
	outFile.open(this->output);
	if (!outFile.is_open()) {
		logger->error("Failed to open file: {}", this->output.string().c_str());
		return;
	}
	outFile << "/* Variable declarations */" << std::endl;
	// TODO: group variable declaration
	for (auto &varId : varDecls.getVars()) {
		outFile << varDecls.type << " " << varId << ";" << std::endl;
	}

	outFile.close();

	// Apply the codegen on each scop
	IPass::apply(prog);
}

void CloogCodegenPass::generateStmtMacro(pesto::OslScop &nest) {
	using metadata::MacroMetadata;
	osl_scop_p scop = nest.c_scop();

	osl_statement_p statement = scop->statement;

	auto macrosData = nest.metadata.getOrCreate<MacroMetadata>().lock();

	int idx = 0;
	while (statement != NULL) {
		osl_body_p body = (osl_body_p)osl_generic_lookup(statement->extension, OSL_URI_BODY);
		if (body == NULL) {
			continue;
		}
		int it_size = osl_strings_size(body->iterators);
		// osl statements are 1-indexed
		std::string args = std::string("");
		for (int i = 0; i < it_size; i += 1) {
			if (i != 0) {
				args += ", ";
			}
			args += body->iterators->string[i];
		}
		char *str = osl_strings_sprint(body->expression);
		std::string stmtBody = std::string(str);
		macrosData->defineFunc("S" + std::to_string(idx + 1), args, stmtBody);

		free(str);

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
		logger->error("An error occured, ClastData was not generated...");
		return;
	}

	// TODO: allow user to customize cloog options
	root = clastData->root;
	state = clastData->cloogState;
	options = clastData->cloogOptions;
	program = clastData->cloogProgram;

	if (root == nullptr || state == nullptr || options == nullptr || program == nullptr) {
		logger->error("Error during Cloog AST generation");
		return;
	}

	logger->info("Writing code to {}", path.c_str());
	FILE *file = fopen(path.c_str(), "a");
	// generate stmt macros
	generateStmtMacro(nest);

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

	// undefine macros
	if (macrosData) {
		std::vector<std::string> macros = macrosData->getDefines();
		for (auto &macro : macros) {
			fprintf(file, "#undef %s\n", macro.c_str());
		}
	}
	fclose(file);
}

pesto::pass::metadata::VarDeclMetadata CloogCodegenPass::mergeVarDecls(Program &prog) {
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

void pesto::pass::CloogCodegenPass::setGenClastPass(std::unique_ptr<pesto::pass::GenClastPass> pass) {
	this->genClastPass = std::move(pass);
}

pesto::pass::metadata::MacroMetadata::MacroMetadata() : list(), defines() {}
pesto::pass::metadata::MacroMetadata::~MacroMetadata() {}

void pesto::pass::metadata::MacroMetadata::define(std::string name, std::string value) {
	std::string macro = "#define " + name + " " + value;
	this->list.push_back(macro);
	this->defines.push_back(name);
}
void pesto::pass::metadata::MacroMetadata::defineFunc(std::string name, std::string args, std::string value) {
	std::string macro = "#define " + name + "(" + args + ") " + value;
	this->list.push_back(macro);
	this->defines.push_back(name);
}