/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <fstream>
#include <iostream>

#include <pesto/bundle/byproduct/OutputSourceFile.hpp>
#include <pesto/clast_support.h>
#include <pesto/ir/ClastIR.hpp>
#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/metadata/Macro.hpp>
#include <pesto/metadata/VarDecl.hpp>
#include <pesto/pass/CodegenPasses.hpp>
#include <pesto/pass/clast/GenClastPass.hpp>

using namespace pesto;
using namespace pesto::pass;

CloogCodegenPass::CloogCodegenPass() : IPass("codegen-cloog"), genClastPass() {}

CloogCodegenPass::~CloogCodegenPass() {}

std::set<std::string> CloogCodegenPass::inputIR() const {
	return {pesto::ir::OpenScopIR::getID(), pesto::ir::ClastIR::getID()};
}

std::string CloogCodegenPass::outputIR() const {
	return "";
}

Bundle CloogCodegenPass::apply(Bundle &&bundle) {
	std::filesystem::path outputPath(".output.cloog.c");
	std::string path = outputPath.string();

	if (bundle.ir_id() == pesto::ir::OpenScopIR::getID()) {
		logger->info("Applying GenClastPass to generate Cloog AST from OpenScopIR");
		bundle = this->genClastPass->apply(std::move(bundle));
	}

	if (bundle.ir_id() != pesto::ir::ClastIR::getID()) {
		logger->error("CloogCodegenPass: expected ClastIR after GenClastPass, got {}", bundle.ir_id().c_str());
		throw std::runtime_error("CloogCodegenPass: expected ClastIR after GenClastPass");
	}

	auto &clastIR = dynamic_cast<ir::ClastIR &>(*bundle.get_ir());
	CloogProgram *program = clastIR.getCloogProgram();
	CloogOptions *options = clastIR.getCloogOptions();
	struct clast_stmt *root = clastIR.get();

	// Generate variable declarations
	auto &varDecl = bundle.metadata.get<pesto::bundle::metadata::VarDecl>();

	// update vardecl metadata

	// Add iterators to var decls
	for (int i = 0; i < program->names->nb_scattering; i += 1) {
		varDecl.addVar(program->names->scattering[i]);
	}

	logger->info("Writing var decls to {}", outputPath.string().c_str());

	std::ofstream outFile;
	outFile.open(outputPath, std::ios::out);
	if (!outFile.is_open()) {
		logger->error("Failed to open file: {}", outputPath.string().c_str());
		throw std::runtime_error("Failed to open output file for writing variable declarations");
	}

	outFile.close();

	assert(options != nullptr);
	assert(program != nullptr);
	assert(root != nullptr);

	logger->info("Writing code to {}", path.c_str());

	FILE *file = fopen(path.c_str(), "a");
	// then print cloog ast
	clast_pprint(file, root, 0, options);

	fclose(file);

	bundle.set_ir(nullptr);
	bundle.add_byproduct(std::make_unique<pesto::bundle::byproduct::OutputSourceFile>(outputPath));
	return std::move(bundle);
}

void pesto::pass::CloogCodegenPass::setGenClastPass(std::unique_ptr<pesto::pass::GenClastPass> pass) {
	this->genClastPass = std::move(pass);
}
