/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <fstream>
#include <iostream>

#include <pesto/PassBuilder.hpp>
#include <pesto/passbuilder/ClastCustomVecPragmaPB.hpp>
#include <pesto/passbuilder/ClastParamTilingPB.hpp>
#include <pesto/passbuilder/CloogCodegenPB.hpp>
#include <pesto/passbuilder/DumpOsl.hpp>
#include <pesto/passbuilder/GenClastPB.hpp>
#include <pesto/passbuilder/IslPB.hpp>
#include <pesto/passbuilder/MatchCriteriaPB.hpp>
#include <pesto/passbuilder/ParamContextPB.hpp>
#include <pesto/passbuilder/PlutoPB.hpp>
#include <pesto/passbuilder/TpzTilingPB.hpp>
#include <pesto/passbuilder/SkewPB.hpp>

using namespace pesto::builder;

PassManagerBuilder::PassManagerBuilder() {
	logger = log::get_logger("pm-builder");
}

PassManagerBuilder::~PassManagerBuilder() {}

std::unique_ptr<PassManagerBuilder> PassManagerBuilder::createDefault() {
	std::unique_ptr<PassManagerBuilder> pm = std::make_unique<PassManagerBuilder>();

	// Register the pass builders
	pm->registerPassBuilder(GenClastPB::getID(), std::make_unique<GenClastPB>());
	pm->registerPassBuilder(CloogCodegenPB::getID(), std::make_unique<CloogCodegenPB>());
	pm->registerPassBuilder(PlutoPB::getID(), std::make_unique<PlutoPB>());
	pm->registerPassBuilder(ClastParamTilingPB::getID(), std::make_unique<ClastParamTilingPB>());
	pm->registerPassBuilder(ParamContextPB::getID(), std::make_unique<ParamContextPB>());
	pm->registerPassBuilder(MatchCriteriaPB::getID(), std::make_unique<MatchCriteriaPB>());
	pm->registerPassBuilder(ClastCustomVecPragmaPB::getID(), std::make_unique<ClastCustomVecPragmaPB>());
	pm->registerPassBuilder(OslDumpPB::getID(), std::make_unique<OslDumpPB>());
	pm->registerPassBuilder(IslPB::getID(), std::make_unique<IslPB>());
	pm->registerPassBuilder(TpzTilingPB::getID(), std::make_unique<TpzTilingPB>());
	pm->registerPassBuilder(SkewPB::getID(), std::make_unique<SkewPB>());

	return pm;
}

void PassManagerBuilder::registerPassBuilder(std::string uuid, std::unique_ptr<IPassBuilder> pb) {
	// Register the pass builder
	builders[uuid] = std::move(pb);
}

std::unique_ptr<pesto::PassManager> PassManagerBuilder::build(std::string path) {
	logger->info("Building pass manager from {}", path.c_str());
	logger->info("Number of builders registered: {}", builders.size());

	// Parse the JSON file and build the pass manager
	std::ifstream file(path);

	if (!file.is_open()) {
		logger->error("Could not open file {}", path.c_str());
		return nullptr;
	}

	json j;
	try {
		file >> j;
	} catch (json::parse_error &e) {
		logger->error("Could not parse JSON file {}", path.c_str());
		return nullptr;
	}

	file.close();

	std::unique_ptr<PassManager> pm = std::make_unique<PassManager>();

	try {
		if (!j.at("passes").is_array()) {
			throw std::invalid_argument("Configuration field 'passes' must be an array");
		}
		for (const auto &json_pass : j.at("passes")) {
			auto pass = this->buildPass(json_pass);
			if (pass) {
				pm->addPass(std::move(pass));
			}
		}
	} catch (const json::out_of_range &e) {
		logger->error("JSON key error {}", e.what());
		return nullptr;
	}

	return pm;
}

bool PassManagerBuilder::hasPassBuilder(std::string uuid) {
	// Check if a pass builder is registered with the given uuid
	return builders.find(uuid) != builders.end();
}

std::unique_ptr<pesto::pass::IPass> PassManagerBuilder::buildPass(const json &j) {
	// Build a pass from a JSON object
	if (j.contains("uuid") == false) {
		throw std::invalid_argument("Each pass must contain a uuid field");
	}
	std::string uuid = j["uuid"];
	auto builder = this->getPassBuilder(uuid);
	if (!builder.has_value()) {
		logger->error("Could not find pass builder with uuid {}", uuid.c_str());
		throw std::runtime_error("Parsing error: unknown pass uuid");
	}

	std::unique_ptr<pesto::pass::IPass> pass = builder->get().build(*this, j);

	if (!pass) {
		logger->error("Pass builder with uuid {} failed to build the pass", uuid.c_str());
		throw std::runtime_error("Pass builder failed to build the pass");
	}

	logger->info("Pass '{}' built", uuid.c_str());

	return pass;
}

std::optional<std::reference_wrapper<IPassBuilder>> PassManagerBuilder::getPassBuilder(std::string uuid) {
	auto it = builders.find(uuid);
	if (it != builders.end()) {
		return std::ref(*it->second);
	}
	return std::nullopt;
}
