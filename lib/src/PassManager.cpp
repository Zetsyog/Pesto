/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/Logging.hpp>
#include <pesto/PassManager.hpp>
#include <pesto/macros.h>

using namespace pesto;

PassManager::PassManager() {
	logger = log::get_logger("pass-manager");
}

PassManager::~PassManager() {}

Bundle PassManager::run(Bundle &&bundle) {
	if (bundle.empty()) {
		logger->warn("PassManager: empty bundle, skipping optimization passes");
		return std::move(bundle);
	}

	Bundle current = std::move(bundle);
	for (const auto &pass : this->passList) {
		if (current.empty()) {
			logger->warn("PassManager: empty bundle after pass '{}', skipping remaining passes",
						 pass->getName().c_str());
			return current;
		}
		if (pass->inputIR().find(current.get_ir()->id()) == pass->inputIR().end()) {
			logger->error("Pass '{}' cannot be applied to IR '{}'", pass->getName().c_str(),
						  current.get_ir()->id().c_str());
			throw std::runtime_error("PassManager: incompatible pass and IR");
		}
		logger->info("Applying pass '{}'", pass->getName().c_str());
		current = pass->apply(std::move(current));
	}
	return current;
}

void PassManager::addPass(std::unique_ptr<pass::IPass> pass) {
	this->passList.push_back(std::move(pass));
}
