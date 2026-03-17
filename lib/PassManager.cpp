/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <iostream>
#include <pesto/PassManager.hpp>
#include <pesto/macros.h>

using namespace pesto;

PassManager::PassManager() {}

PassManager::~PassManager() {}

void PassManager::run(Program &prog) {
	for (auto it = this->passList.begin(); it != this->passList.end(); ++it) {
		int nestCount = prog.getScopNests().size();
		std::cerr << "[" << PESTO_NAME_PM << "] Running pass '" << (*it)->getName() << "' on "
				  << std::to_string(nestCount) << " loop nests " << std::endl;

		(*it)->apply(prog);
	}
}

void PassManager::addPass(std::unique_ptr<pass::IPass> pass) { this->passList.push_back(std::move(pass)); }
