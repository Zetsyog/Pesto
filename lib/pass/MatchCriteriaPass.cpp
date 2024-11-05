#include <regex>

#include <pesto/OslPass.hpp>

#include "osl_support.h"

#include <muParser.h>

using pesto::pass::IPass;
using pesto::pass::MatchCriteriaPass;

MatchCriteriaPass::MatchCriteriaPass() : pesto::pass::IPass("if-pass") {
	// initialize the symbol table
}

MatchCriteriaPass::~MatchCriteriaPass() {}

void MatchCriteriaPass::addThenPass(std::unique_ptr<IPass> pass) {
	this->thenPasses.push_back(std::move(pass));
}

void MatchCriteriaPass::addElsePass(std::unique_ptr<IPass> pass) {
	this->elsePasses.push_back(std::move(pass));
}

void MatchCriteriaPass::setCriteria(std::string criteria) {
	this->criteria = criteria;
}

void MatchCriteriaPass::applyScop(OslScop &nest) {

	std::unordered_map<std::string, double> variables = {{"scop_max_depth", 2},
														 {"stmt_nb", 1}};

	try {
		double var_a = 1;
		mu::Parser p;
		p.DefineVar("a", &var_a);
		p.SetExpr("MySqr(a)*_pi+min(10,a)");

		} catch (mu::Parser::exception_type &e) {
		std::cout << e.GetMsg() << std::endl;
	}

	// if (result) {
	// 	for (auto &pass : this->thenPasses) {
	// 		pass->applyScop(nest);
	// 	}
	// } else {
	// 	for (auto &pass : this->elsePasses) {
	// 		pass->applyScop(nest);
	// 	}
	// }
}
