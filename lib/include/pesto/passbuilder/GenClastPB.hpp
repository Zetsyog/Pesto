/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <pesto/passbuilder/PassBuilder.hpp>

#include <memory>
#include <string>

namespace pesto {

namespace builder {

class GenClastPB : public IPassBuilder {
  public:
	GenClastPB();
	virtual ~GenClastPB();

	virtual std::unique_ptr<pesto::pass::IPass> build(PassManagerBuilder &builder, const json &j);
};

} // namespace builder

} // namespace pesto
