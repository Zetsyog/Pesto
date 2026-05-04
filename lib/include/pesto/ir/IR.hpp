/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <memory>
#include <string>

namespace pesto::ir {

// Base interface for all IRs
class IR {
  public:
	virtual ~IR() = default;

	// Return a unique, human-readable IR name (e.g., "OSL", "CloogAST")
	virtual const std::string &id() const = 0;
};

} // namespace pesto::ir
