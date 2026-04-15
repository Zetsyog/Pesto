/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <string>

#include <pesto/pass/Pass.hpp>

using namespace pesto::pass;

IPass::IPass(std::string name) : name(name) { logger = log::get_logger("pass/" + name); }

IPass::~IPass() {}

/**
 * Call the applyScop method on each ScopNest of the Program
 * @param prog the Program to run the pass on
 */
void IPass::apply(pesto::Program &prog) {
	std::vector<OslScop *> nests = prog.getScopNests();
	for (auto &nest : nests) {
		this->applyScop(*nest);
	}
}

void IPass::applyScop(pesto::OslScop &nest) { (void)nest; }