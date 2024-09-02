#include <fstream>
#include <iostream>

#include <pesto/Pesto.hpp>

using namespace pesto;

PMBuilder::PMBuilder() {}

PMBuilder::~PMBuilder() {}

std::unique_ptr<PMBuilder> PMBuilder::createDefault() {
	std::unique_ptr<PMBuilder> pm = std::make_unique<PMBuilder>();

	// Register the pass builders
	pm->registerPassBuilder(std::make_unique<GenClastPB>());
	pm->registerPassBuilder(std::make_unique<CloogCodegenPB>());
	pm->registerPassBuilder(std::make_unique<PlutoPB>());
	pm->registerPassBuilder(std::make_unique<SplitNestPB>());
	pm->registerPassBuilder(std::make_unique<ClastParamTilingPB>());

	return pm;
}

void PMBuilder::registerPassBuilder(std::unique_ptr<IPassBuilder> pb) {
	// Register the pass builder
	builders[pb->getUUID()] = std::move(pb);
}

std::unique_ptr<PassManager> PMBuilder::fromJSON(std::string path) {
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
		for (const auto &pass : j.at("passes")) {
			std::string uuid = pass.at("uuid");

			auto builder = builders.find(uuid);
			if (builder == builders.end()) {
				std::cerr << "Error: could not find pass builder with uuid "
						  << uuid << std::endl;
				continue;
			}
			pm->addPass(builder->second->build(pass));
		}
	} catch (const json::out_of_range &e) {
		std::cerr << "Error: JSON key error " << e.what() << std::endl;
		return nullptr;
	}

	return pm;
}