#include <pesto/pass/OslPass.hpp>
#include <pesto/passbuilder/MatchCriteriaPB.hpp>

using namespace pesto::builder;

using pesto::pass::MatchCriteriaPass;

MatchCriteriaPB::MatchCriteriaPB() {}

MatchCriteriaPB::~MatchCriteriaPB() {}

std::unique_ptr<pesto::pass::IPass> MatchCriteriaPB::build(PassManagerBuilder &builder, const json &j) {
	auto pass = std::make_unique<MatchCriteriaPass>();

	if (!j.contains("criteria")) {
		throw std::runtime_error("MatchCriteriaPB: criteria field is required");
	}
	pass->setCriteria(j["criteria"]);
	if (j.contains("then")) {
		for (auto &thenPass : j["then"]) {
			pass->addThenPass(builder.buildPass(thenPass));
		}
	}
	if (j.contains("else")) {
		for (auto &elsePass : j["else"]) {
			pass->addElsePass(builder.buildPass(elsePass));
		}
	}

	return pass;
}
