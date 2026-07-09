/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <regex>

#include <pesto/pass/MatchCriteria.hpp>

#include <pesto/osl_support.h>

using pesto::Bundle;
using pesto::pass::IPass;
using pesto::pass::MatchCriteriaPass;

MatchCriteriaPass::MatchCriteriaPass() : pesto::pass::IPass("if") {
	this->variables = {{"scop_max_it", 0}, {"stmt_nb", 0}};

	// Add the variables to the parser
	try {
		for (const auto &[name, value] : this->variables) {
			this->parser.DefineVar(name, const_cast<double *>(&value));
		}
	} catch (mu::Parser::exception_type &e) {
		logger->error("Error defining variables: {}", e.GetMsg().c_str());
	}
}

MatchCriteriaPass::~MatchCriteriaPass() {}

void MatchCriteriaPass::addThenPass(std::unique_ptr<IPass> pass) {
	this->thenPassManager.addPass(std::move(pass));
}

void MatchCriteriaPass::addElsePass(std::unique_ptr<IPass> pass) {
	this->elsePassManager.addPass(std::move(pass));
}

void MatchCriteriaPass::setCriteria(std::string criteria) {
	this->criteria = criteria;

	try {
		parser.SetExpr(this->criteria);
	} catch (mu::Parser::exception_type &e) {
		logger->error("Error parsing criteria: {}", e.GetMsg().c_str());
	}

	logger->debug("criteria: {}", this->criteria.c_str());
}

Bundle MatchCriteriaPass::apply(Bundle &&bundle) {
	auto *scopIR = dynamic_cast<pesto::ir::OpenScopIR *>(bundle.get_ir());
	osl_scop_p scop = scopIR->c_scop();

	this->variables["stmt_nb"] = osl_statement_number(scop->statement);

	int max_it = 0;
	// count max number of iterator
	osl_statement_p stmt = scop->statement;
	for (; stmt != NULL; stmt = stmt->next) {
		max_it = stmt->domain->nb_output_dims > max_it ? stmt->domain->nb_output_dims : max_it;
	}
	this->variables["scop_max_it"] = max_it;

	bool result;

	try {
		result = this->parser.Eval() != 0;
	} catch (mu::Parser::exception_type &e) {
		logger->error("Error evaluating criteria: {}", e.GetMsg().c_str());
		throw std::runtime_error("Error evaluating criteria");
	}

	if (result) {
		logger->info("Criteria matched. Applying then passes");
		return this->thenPassManager.run(std::move(bundle));
	} else {
		logger->info("Criteria not matched. Applying else passes");
		return this->elsePassManager.run(std::move(bundle));
	}
}
