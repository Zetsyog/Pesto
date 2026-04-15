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

PassManager::PassManager() { logger = log::get_logger("pass-manager"); }

PassManager::~PassManager() {}

void PassManager::run(Program &prog) {
	for (auto it = this->passList.begin(); it != this->passList.end(); ++it) {
		int nestCount = prog.getScopNests().size();
		logger->info("Running pass '{}' on {} loop nests", (*it)->getName().c_str(), nestCount);

		(*it)->apply(prog);
	}
}

void PassManager::addPass(std::unique_ptr<pass::IPass> pass) { this->passList.push_back(std::move(pass)); }
