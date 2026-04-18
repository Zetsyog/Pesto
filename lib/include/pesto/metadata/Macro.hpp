/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <string>
#include <vector>

#include <pesto/Utils.hpp>

namespace pesto::bundle::metadata {
/**
 * A metadata that holds a list of macros that will be included at the top of
 * the generated C code
 */
struct Macro {
	std::vector<std::string> list;

	void define(std::string name, std::string value) {
		this->list.push_back("#define " + name + " " + value);
	}

	void defineFunc(std::string name, std::string args, std::string value) {
		this->list.push_back("#define " + name + "(" + args + ") " + value);
	}
};
} // namespace pesto::bundle::metadata
