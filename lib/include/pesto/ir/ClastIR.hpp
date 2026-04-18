/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <memory>
#include <vector>

#include <cloog/cloog.h>

#include <pesto/ir/IR.hpp>

namespace pesto {
namespace ir {

class ClastIR : public IR {
  public:
	static const std::string ID;

	std::vector<std::shared_ptr<std::string>> names;

	ClastIR(CloogState *cloogState, CloogOptions *cloogOptions, CloogProgram *cloogProgram, struct clast_stmt *root,
			unsigned int stmtsNb)
		: names()
		, cloogState_(cloogState)
		, cloogOptions_(cloogOptions)
		, cloogProgram_(cloogProgram)
		, root_(root)
		, stmtsNb_(stmtsNb) {}
	virtual ~ClastIR();

	const std::string &id() const override {
		return ID;
	}

	CloogState *getCloogState() const {
		return cloogState_;
	}
	CloogOptions *getCloogOptions() const {
		return cloogOptions_;
	}
	CloogProgram *getCloogProgram() const {
		return cloogProgram_;
	}
	struct clast_stmt *get() const {
		return root_;
	}

	unsigned int getStmtsNb() const {
		return stmtsNb_;
	}

  private:
	CloogState *cloogState_;
	CloogOptions *cloogOptions_;
	CloogProgram *cloogProgram_;

	struct clast_stmt *root_;

	unsigned int stmtsNb_;
};

} // namespace ir
} // namespace pesto
