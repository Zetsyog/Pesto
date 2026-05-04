/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <memory>
#include <set>
#include <string>

namespace pesto::bundle::metadata {

class VarDecl {
  public:
	std::string typeName = "long int";

	void addVar(const std::string &var) {
		this->vars_.insert(var);
	}

	const std::set<std::string> &vars() const {
		return this->vars_;
	}

  private:
	std::set<std::string> vars_;
};

} // namespace pesto::bundle::metadata
