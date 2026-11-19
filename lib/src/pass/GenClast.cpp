/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <iostream>

#include <pesto/clast_support.h>
#include <pesto/ir/ClastIR.hpp>
#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/metadata/Macro.hpp>
#include <pesto/metadata/VarDecl.hpp>
#include <pesto/osl_support.h>
#include <pesto/pass/CloogCodegen.hpp>
#include <pesto/pass/clast/GenClast.hpp>

using namespace pesto::pass;
using namespace pesto;

GenClastPass::GenClastPass()
	: IPass("clast-gen")
	, codegenContext(-1)
	, cloogL(-1)
	, cloogF(1)
	, cloogStop(-1)
	, cloogSh(0)
	, cloogFirstUnroll(-1)
	, cloogBacktrack(0)
	, cloogNoSimplify(0)
	, cloogEsp(1) {}

GenClastPass::~GenClastPass() {}

Bundle GenClastPass::apply(Bundle &&bundle) {
	ir::OpenScopIR *scopIR = dynamic_cast<ir::OpenScopIR *>(bundle.get_ir());
	auto &varDecl = bundle.metadata.get<pesto::bundle::metadata::VarDecl>();
	auto &macro = bundle.metadata.get<pesto::bundle::metadata::Macro>();

	osl_scop_p scop = scopIR->c_scop();

	// Declare scatterting iterators as variables
	osl_scatnames_p scatnames = (osl_scatnames_p)osl_generic_lookup(scop->extension, OSL_URI_SCATNAMES);
	if (scatnames != nullptr) {
		for (size_t i = 0; i < osl_strings_size(scatnames->names); i++) {
			varDecl.addVar(scatnames->names->string[i]);
		}
	}

	// Add macro for statements
	osl_statement_p statement = scop->statement;

	int idx = 0;
	while (statement != NULL) {
		osl_body_p body = (osl_body_p)osl_generic_lookup(statement->extension, OSL_URI_BODY);
		if (body == NULL) {
			throw std::runtime_error("Clast generation requires a body for every statement");
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
		macro.defineFunc("S" + std::to_string(idx + 1), args, stmtBody);
		logger->debug("Defined macro S{}({}) as '{}'", idx + 1, args.c_str(), stmtBody.c_str());

		free(str);

		idx += 1;
		statement = statement->next;
	}

	CloogState *cloogState = cloog_state_malloc();
	CloogOptions *cloogOptions = cloog_options_malloc(cloogState);

	unsigned int stmtsNb = osl_statement_number(scop->statement);

	// Sanity checks for statement-wise optimization levels
	if (this->stmtFirstOptLevel.size() > 0) {
		if (this->stmtLastOptLevel.size() != stmtsNb) {
			logger->error(
				"stmtLastOptLevel size must be equal to the number of statements in the scop. Found {}, expected {}.",
				this->stmtLastOptLevel.size(), stmtsNb);
			throw std::invalid_argument("stmtLastOptLevel size must be equal to the number of statements in the scop.");
		}
		if (this->stmtFirstOptLevel.size() != stmtsNb) {
			logger->error(
				"stmtFirstOptLevel size must be equal to the number of statements in the scop. Found {}, expected {}.",
				this->stmtFirstOptLevel.size(), stmtsNb);
			throw std::invalid_argument("stmtFirstOptLevel size must be equal to the number of statements in the scop");
		}
		if (this->stmtFirstOptLevel.size() != this->stmtLastOptLevel.size()) {
			logger->error("stmtFirstOptLevel and stmtLastOptLevel must have the same size. First: {}, Last: {}.",
						  this->stmtFirstOptLevel.size(), this->stmtLastOptLevel.size());
			throw std::invalid_argument("stmtFirstOptLevel and stmtLastOptLevel must have the same size");
		}
		if (this->stmtFirstOptLevel.size() != stmtsNb) {
			logger->error("stmtFirstOptLevel and stmtLastOptLevel size must be equal to the number of statements in "
						  "the scop. Found {}, expected {}.",
						  this->stmtFirstOptLevel.size(), stmtsNb);
			throw std::invalid_argument(
				"stmtFirstOptLevel and stmtLastOptLevel size must be equal to the number of statements in the scop");
		}

		logger->info("Using statement-wise optimization levels");
	}

	// Use statement-wise optimization levels if provided, otherwise use the global ones
	if (this->stmtFirstOptLevel.size() > 0) {
		cloogOptions->fs_ls_size = this->stmtFirstOptLevel.size();

		cloogOptions->ls = (int *)malloc(cloogOptions->fs_ls_size * sizeof(int));
		cloogOptions->fs = (int *)malloc(cloogOptions->fs_ls_size * sizeof(int));

		for (int i = 0; i < cloogOptions->fs_ls_size; i++) {
			cloogOptions->fs[i] = this->stmtFirstOptLevel[i];
			cloogOptions->ls[i] = this->stmtLastOptLevel[i];
		}
	} else {
		cloogOptions->l = cloogL;
		cloogOptions->f = cloogF;
	}

	cloogOptions->stop = cloogStop;
	cloogOptions->sh = cloogSh;
	cloogOptions->first_unroll = cloogFirstUnroll;
	cloogOptions->backtrack = cloogBacktrack;
	cloogOptions->nosimplify = cloogNoSimplify;
	cloogOptions->esp = cloogEsp;
	cloogOptions->strides = 1;

	// data.cloogOptions->override = 1;

	cloogOptions->save_domains = 1;
	cloogOptions->openscop = 1;
	cloogOptions->language = CLOOG_LANGUAGE_C;

	if (this->codegenContext != -1) {
		for (int i = 0; i < scop->context->nb_parameters; i += 1) {
			osl_relation_insert_blank_row(scop->context, -1);
			osl_int_set_si(scop->context->precision, &scop->context->m[scop->context->nb_rows - 1][0], 1);
			osl_int_set_si(scop->context->precision,
						   &scop->context->m[scop->context->nb_rows - 1][1 + scop->context->nb_input_dims +
																		 scop->context->nb_local_dims +
																		 scop->context->nb_output_dims + i],
						   1);
			osl_int_set_si(scop->context->precision,
						   &scop->context->m[scop->context->nb_rows - 1]
											[1 + scop->context->nb_input_dims + scop->context->nb_local_dims +
											 scop->context->nb_output_dims + scop->context->nb_parameters],

						   -this->codegenContext);
		}
	}

	CloogInput *input = cloog_input_from_osl_scop(cloogState, scop);
	// input is freed by cloog_clast_create_from_input
	CloogProgram *program = nullptr;

	program = cloog_program_alloc(input->context, input->ud, cloogOptions);
	free(input);

	program = cloog_program_generate(program, cloogOptions);

	struct clast_stmt *root = cloog_clast_create(program, cloogOptions);
	clast_annotate_loops(scop, root);

	auto clastIR = std::make_unique<ir::ClastIR>(cloogState, cloogOptions, program, root, stmtsNb);
	bundle.set_ir(std::move(clastIR));
	return std::move(bundle);
}
