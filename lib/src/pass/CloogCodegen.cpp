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
#include <pesto/pass/CloogCodegen.hpp>
#include <pesto/pass/clast/GenClast.hpp>
#include <pesto/utils/TemporaryDirectory.hpp>

using namespace pesto;
using namespace pesto::pass;

CloogCodegenPass::CloogCodegenPass() : IPass("codegen-cloog"), genClastPass(std::make_unique<GenClastPass>()) {}

CloogCodegenPass::~CloogCodegenPass() {}

std::set<std::string> CloogCodegenPass::inputIR() const {
	return {pesto::ir::OpenScopIR::getID(), pesto::ir::ClastIR::getID()};
}

std::string CloogCodegenPass::outputIR() const {
	return "";
}

Bundle CloogCodegenPass::apply(Bundle &&bundle) {
	// Keep the workspace alive with the bundle, including for library callers.
	auto &workspace = bundle.metadata.get<std::shared_ptr<utils::TemporaryDirectory>>();
	if (!workspace) {
		workspace = std::make_shared<utils::TemporaryDirectory>();
	}
	const auto outputPath = workspace->path() / "output.cloog.c";
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
	if (!program || !program->names || !options || !root) {
		throw std::runtime_error("CloogCodegenPass: incomplete Cloog AST");
	}
	// clast_pprint emits these helpers in bounds, but does not define them.
	// Expand them in the private preprocessing stage rather than relying on
	// benchmark headers or adding macros to the user's translation unit.
	auto &macro = bundle.metadata.get<pesto::bundle::metadata::Macro>();
	macro.defineFunc("min", "x,y", "((x) < (y) ? (x) : (y))");
	macro.defineFunc("max", "x,y", "((x) > (y) ? (x) : (y))");
	macro.defineFunc("floord", "n,d", "((n)/(d) - ((n)%(d) != 0 && ((n)<0) != ((d)<0)))");
	macro.defineFunc("ceild", "n,d", "((n)/(d) + ((n)%(d) != 0 && ((n)<0) == ((d)<0)))");

	// Generate variable declarations
	auto &varDecl = bundle.metadata.get<pesto::bundle::metadata::VarDecl>();

	// update vardecl metadata

	// Add iterators to var decls
	for (int i = 0; i < program->names->nb_scattering; i += 1) {
		varDecl.addVar(program->names->scattering[i]);
	}

	logger->info("Writing code to {}", path.c_str());

	FILE *file = fopen(path.c_str(), "w");
	if (!file) {
		throw std::runtime_error("Failed to open generated code file: " + path);
	}
	// then print cloog ast
	clast_pprint(file, root, 0, options);

	const bool writeFailed = ferror(file) != 0;
	if (fclose(file) != 0 || writeFailed) {
		throw std::runtime_error("Failed to write generated code file: " + path);
	}

	bundle.set_ir(nullptr);
	bundle.add_byproduct(std::make_unique<pesto::bundle::byproduct::OutputSourceFile>(outputPath));
	return std::move(bundle);
}

void pesto::pass::CloogCodegenPass::setGenClastPass(std::unique_ptr<pesto::pass::GenClastPass> pass) {
	if (!pass) {
		throw std::invalid_argument("CloogCodegenPass: cannot use a null GenClastPass");
	}
	this->genClastPass = std::move(pass);
}
