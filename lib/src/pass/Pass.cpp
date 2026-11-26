/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <string>

#include <pesto/pass/Pass.hpp>

using namespace pesto::pass;

IPass::IPass(std::string name) : name(name) {
	logger = log::get_logger("pass/" + name);
}

IPass::~IPass() {}
