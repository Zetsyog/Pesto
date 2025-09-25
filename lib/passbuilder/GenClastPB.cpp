#include <pesto/pass/clast/GenClastPass.hpp>
#include <pesto/passbuilder/GenClastPB.hpp>

using namespace pesto::builder;

GenClastPB::GenClastPB() {}

GenClastPB::~GenClastPB() {}

std::unique_ptr<pesto::pass::IPass> GenClastPB::build(PassManagerBuilder &builder, const json &j) {
	(void)builder;
	(void)j;

	auto pass = std::make_unique<pass::GenClastPass>();

	// TODO: cleaner parsing
	for (auto &[key, val] : j.items()) {
		// ignore uuid
		if (key == "uuid") {
		} else if (key == "optLastLevel") {
			if (!val.is_number_integer()) {
				std::cerr << "Warning: " << key << " must be an integer. Ignoring." << std::endl;
				continue;
			}
			pass->cloogL = val.get<int>();
		} else if (key == "optFirstLevel") {
			if (!val.is_number_integer()) {
				std::cerr << "Warning: " << key << " must be an integer. Ignoring." << std::endl;
				continue;
			}
			pass->cloogF = val.get<int>();
		} else if (key == "stopLevel") {
			if (!val.is_number_integer()) {
				std::cerr << "Warning: " << key << " must be an integer. Ignoring." << std::endl;
				continue;
			}
			pass->cloogStop = val.get<int>();
		} else if (key == "simpleHulls") {
			if (!val.is_boolean()) {
				std::cerr << "Warning: " << key << " must be a boolean. Ignoring." << std::endl;
				continue;
			}
			pass->cloogSh = val.get<bool>() ? 1 : 0;
		} else if (key == "firstUnrollDimension") {
			if (!val.is_number_integer()) {
				std::cerr << "Warning: " << key << " must be an integer. Ignoring." << std::endl;
				continue;
			}
			pass->cloogFirstUnroll = val.get<int>();
		} else if (key == "backtrack") {
			if (!val.is_boolean()) {
				std::cerr << "Warning: " << key << " must be a boolean. Ignoring." << std::endl;
				continue;
			}
			pass->cloogBacktrack = val.get<bool>() ? 1 : 0;
		} else if (key == "codegenContext") {
			if (!val.is_number_integer()) {
				std::cerr << "Warning: " << key << " must be an integer. Ignoring." << std::endl;
				continue;
			}
			pass->codegenContext = val.get<int>();
		} else if (key == "spreadEqualities") {
			if (!val.is_boolean()) {
				std::cerr << "Warning: " << key << " must be a boolean. Ignoring." << std::endl;
				continue;
			}
			pass->cloogEsp = val.get<bool>() ? 1 : 0;
		} else {
			std::cerr << "Warning: " << key << " is not a valid option for " << j["uuid"] << ". Ignoring." << std::endl;
		}
	}

	return pass;
}
