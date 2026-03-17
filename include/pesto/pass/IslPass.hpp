/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#ifndef PESTO_ISL_PASS_H
#define PESTO_ISL_PASS_H

#include <isl/ctx.h>

#include <pesto/pass/Pass.hpp>

namespace pesto::pass {

class IslPass : public IPass {
  public:
	IslPass();
	virtual ~IslPass();

	virtual void applyScop(OslScop &nest) override;

  private:
	isl_ctx *ctx;
};

} // namespace pesto::pass

#endif // PESTO_ISL_PASS_H