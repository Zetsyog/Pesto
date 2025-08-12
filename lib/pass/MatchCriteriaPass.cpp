#include <regex>

#include <pesto/pass/OslPass.hpp>

#include <pesto/osl_support.h>

using pesto::pass::IPass;
using pesto::pass::MatchCriteriaPass;

MatchCriteriaPass::MatchCriteriaPass() : pesto::pass::IPass("if-pass") {
	this->variables = {{"scop_max_it", 0}, {"stmt_nb", 0}};

	// Add the variables to the parser
	try {
		for (const auto &[name, value] : this->variables) {
			this->parser.DefineVar(name, const_cast<double *>(&value));
		}
	} catch (mu::Parser::exception_type &e) {
		std::cerr << "Error defining variables: ";
		std::cerr << e.GetMsg() << std::endl;
	}
}

MatchCriteriaPass::~MatchCriteriaPass() {}

void MatchCriteriaPass::addThenPass(std::unique_ptr<IPass> pass) { this->thenPasses.push_back(std::move(pass)); }

void MatchCriteriaPass::addElsePass(std::unique_ptr<IPass> pass) { this->elsePasses.push_back(std::move(pass)); }

void MatchCriteriaPass::setCriteria(std::string criteria) {
	this->criteria = criteria;

	try {
		parser.SetExpr(criteria);
	} catch (mu::Parser::exception_type &e) {
		std::cerr << "Error parsing criteria: ";
		std::cerr << e.GetMsg() << std::endl;
	}

	std::cerr << "criteria: " << criteria << std::endl;
}

void MatchCriteriaPass::applyScop(OslScop &nest) {

	this->variables["stmt_nb"] = osl_statement_number(nest.c_scop()->statement);

	int max_it = 0;
	// count max number of iterator
	osl_statement_p stmt = nest.c_scop()->statement;
	for (; stmt != NULL; stmt = stmt->next) {
		max_it = stmt->domain->nb_output_dims > max_it ? stmt->domain->nb_output_dims : max_it;
	}
	this->variables["scop_max_it"] = max_it;

	bool result;

	try {
		result = this->parser.Eval() != 0;
	} catch (mu::Parser::exception_type &e) {
		std::cerr << e.GetMsg() << std::endl;
		return;
	}

	if (result) {
		log(std::cerr) << "Criteria matched for scop " << nest.getId() << ". Applying then passes" << std::endl;
		for (auto &pass : this->thenPasses) {
			pass->applyScop(nest);
		}
	} else {
		log(std::cerr) << "Criteria not matched for scop " << nest.getId() << ". Applying else passes" << std::endl;
		for (auto &pass : this->elsePasses) {
			pass->applyScop(nest);
		}
	}
}
