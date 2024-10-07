#include <pesto/Pesto.hpp>

using namespace pesto;

ParamContextPB::ParamContextPB() {}

ParamContextPB::~ParamContextPB() {}

std::string ParamContextPB::getUUID() { return PESTO_UUID_PASS_PARAM_CONTEXT; }

std::unique_ptr<pesto::IPestoPass> ParamContextPB::build(PMBuilder &builder,const json &j) {
	// TODO Parse the JSON object and build the pass
	if (!j.contains("value")) {
		std::cerr << "[pass-builder] Missing option \".value\" for pass \""
				  << getUUID() << "\"" << std::endl;
		return std::unique_ptr<pesto::ParamContextPass>{};
	}

	auto pass = std::make_unique<ParamContextPass>(j["value"]);

	return pass;
}