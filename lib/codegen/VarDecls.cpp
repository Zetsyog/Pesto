/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <algorithm>

#include <pesto/pass/CodegenPasses.hpp>

using namespace pesto::pass::metadata;

VarDeclMetadata::VarDeclMetadata() : type("long int") {}

VarDeclMetadata::~VarDeclMetadata() {}

void VarDeclMetadata::addVar(std::string var) {
	if (std::find(vars.begin(), vars.end(), var) != vars.end()) {
		return;
	}
	vars.push_back(var);
}

std::vector<std::string> VarDeclMetadata::getVars() { return vars; }