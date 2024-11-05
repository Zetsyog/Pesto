#include <fstream>
#include <iostream>

#include <pesto/Pesto.hpp>

using namespace pesto::builder;

PassManagerBuilder::PassManagerBuilder() {}

PassManagerBuilder::~PassManagerBuilder() {}

std::unique_ptr<PassManagerBuilder> PassManagerBuilder::createDefault() {
	std::unique_ptr<PassManagerBuilder> pm =
		std::make_unique<PassManagerBuilder>();

	// Register the pass builders
	pm->registerPassBuilder(PESTO_UUID_PASS_CLAST_GEN,
							std::make_unique<GenClastPB>());
	pm->registerPassBuilder(PESTO_UUID_PASS_GEN_CLOOG,
							std::make_unique<CloogCodegenPB>());
	pm->registerPassBuilder(PESTO_UUID_PASS_PLUTO_GENERIC,
							std::make_unique<PlutoPB>());
	pm->registerPassBuilder(PESTO_UUID_PASS_SPLIT_NESTED_LOOPS,
							std::make_unique<SplitNestPB>());
	pm->registerPassBuilder(PESTO_UUID_PASS_PARAM_TILING,
							std::make_unique<ClastParamTilingPB>());
	pm->registerPassBuilder(PESTO_UUID_PASS_PARAM_CONTEXT,
							std::make_unique<ParamContextPB>());
	pm->registerPassBuilder(PESTO_UUID_PASS_MATCH_CRITERIA,
							std::make_unique<MatchCriteriaPB>());

	return pm;
}

void PassManagerBuilder::registerPassBuilder(std::string uuid,
											 std::unique_ptr<IPassBuilder> pb) {
	// Register the pass builder
	builders[uuid] = std::move(pb);
}

std::unique_ptr<pesto::PassManager>
PassManagerBuilder::build(std::string path) {
	std::cerr << "[pesto-pmbuilder] Building pass manager from " << path
			  << std::endl;
	std::cerr << "[pesto-pmbuilder] " << builders.size()
			  << " builders registered" << std::endl;

	// Parse the JSON file and build the pass manager
	std::ifstream file(path);

	if (!file.is_open()) {
		std::cerr << "Error: could not open file " << path << std::endl;
		return nullptr;
	}

	json j;
	try {
		file >> j;
	} catch (json::parse_error &e) {
		std::cerr << "Error: could not parse JSON file " << path << std::endl;
		return nullptr;
	}

	file.close();

	std::unique_ptr<PassManager> pm = std::make_unique<PassManager>();

	try {
		for (const auto &json_pass : j.at("passes")) {
			auto pass = this->buildPass(json_pass);
			if (pass) {
				pm->addPass(std::move(pass));
			}
		}
	} catch (const json::out_of_range &e) {
		std::cerr << "Error: JSON key error " << e.what() << std::endl;
		return nullptr;
	}

	return pm;
}

bool PassManagerBuilder::hasPassBuilder(std::string uuid) {
	// Check if a pass builder is registered with the given uuid
	return builders.find(uuid) != builders.end();
}

std::unique_ptr<pesto::pass::IPass>
PassManagerBuilder::buildPass(const json &j) {
	// Build a pass from a JSON object
	if (j.contains("uuid") == false) {
		std::cerr << "Error: can't parse pass. JSON object does not contain a "
					 "uuid field"
				  << std::endl;
		return nullptr;
	}
	std::string uuid = j["uuid"];
	if (this->hasPassBuilder(uuid) == false) {
		std::cerr << "Error: could not find pass builder with uuid " << uuid
				  << std::endl;
		return nullptr;
	}

	std::unique_ptr<pesto::pass::IPass> pass = builders[uuid]->build(*this, j);

	std::cerr << "[pesto-pmbuilder] Pass '" << uuid << "' built" << std::endl;

	return pass;
}