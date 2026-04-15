/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <iostream>

#include <pesto/clast_support.h>
#include <pesto/osl_support.h>
#include <pesto/pass/CodegenPasses.hpp>
#include <pesto/pass/clast/GenClastPass.hpp>

using namespace pesto::pass;

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

void GenClastPass::applyScop(OslScop &nest) {
	using metadata::ClastHolder;
	using metadata::VarDeclMetadata;

	auto data = nest.metadata.getOrCreate<ClastHolder>().lock();
	// TODO: add an option to force clast generation? (and erase old clast)
	if (data->cloogState != nullptr || data->cloogOptions != nullptr || data->cloogProgram != nullptr ||
		data->root != nullptr) {
		logger->info("Clast already generated for nest {}. Skipping.", nest.getId());
		return;
	}

	osl_scop_p scop = nest.c_scop();

	data->cloogState = cloog_state_malloc();
	data->cloogOptions = cloog_options_malloc(data->cloogState);
	data->stmtsNb = osl_statement_number(scop->statement);

	data->cloogOptions->l = cloogL;
	data->cloogOptions->f = cloogF;
	data->cloogOptions->stop = cloogStop;
	data->cloogOptions->sh = cloogSh;
	data->cloogOptions->first_unroll = cloogFirstUnroll;
	data->cloogOptions->backtrack = cloogBacktrack;
	data->cloogOptions->nosimplify = cloogNoSimplify;
	data->cloogOptions->esp = cloogEsp;
	data->cloogOptions->strides = 1;

	// data.cloogOptions->override = 1;

	data->cloogOptions->save_domains = 1;
	data->cloogOptions->openscop = 1;
	data->cloogOptions->language = CLOOG_LANGUAGE_C;

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

	CloogInput *input = cloog_input_from_osl_scop(data->cloogState, scop);
	// input is freed by cloog_clast_create_from_input
	CloogProgram *program;

	program = cloog_program_alloc(input->context, input->ud, data->cloogOptions);
	free(input);

	program = cloog_program_generate(program, data->cloogOptions);

	data->cloogProgram = program;

	data->root = cloog_clast_create(program, data->cloogOptions);
	clast_annotate_loops(scop, data->root);

	// update vardecl metadata
	auto varDecls = nest.metadata.getOrCreate<VarDeclMetadata>().lock();

	// Add iterators to var decls
	for (int i = 0; i < program->names->nb_scattering; i += 1) {
		varDecls->addVar(program->names->scattering[i]);
	}

	osl_scatnames_p scatnames = (osl_scatnames_p)osl_generic_lookup(nest.c_scop()->extension, OSL_URI_SCATNAMES);
	if (scatnames != nullptr) {
		for (size_t i = 0; i < osl_strings_size(scatnames->names); i++) {
			varDecls->addVar(scatnames->names->string[i]);
		}
	}
}

metadata::ClastHolder::ClastHolder()
	: cloogState(nullptr), cloogOptions(nullptr), cloogProgram(nullptr), root(nullptr) {}

metadata::ClastHolder::~ClastHolder() {
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