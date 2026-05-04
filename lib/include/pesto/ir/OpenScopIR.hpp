/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <osl/scop.h> // or forward + extern "C"

#include <pesto/ir/IR.hpp>

namespace pesto {
namespace ir {

class OpenScopIR : public IR {
  public:
	static const std::string ID;

	OpenScopIR(osl_scop_p s) : scop_(s, osl_scop_free) {}

	const std::string &id() const override {
		return ID;
	}

	osl_scop_p c_scop() const {
		return scop_.get();
	}

  private:
	std::shared_ptr<osl_scop> scop_;
};

} // namespace ir
} // namespace pesto
