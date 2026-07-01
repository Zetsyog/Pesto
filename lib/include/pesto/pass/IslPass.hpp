/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <isl/ctx.h>

#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/pass/Pass.hpp>
namespace pesto::pass {

class IslPass : public IPass {
  public:
	IslPass();
	virtual ~IslPass();

	virtual Bundle apply(Bundle &&bundle) override;
	virtual std::set<std::string> inputIR() const override {
		return {pesto::ir::OpenScopIR::getID()};
	}
	virtual std::string outputIR() const override {
		return pesto::ir::OpenScopIR::getID();
	}

  private:
	isl_ctx *ctx;
};

} // namespace pesto::pass
