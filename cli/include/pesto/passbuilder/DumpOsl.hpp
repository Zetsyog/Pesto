/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <pesto/PassBuilder.hpp>

#include <memory>

namespace pesto {

namespace builder {

class OslDumpPB : public IPassBuilder {
  public:
	OslDumpPB();
	virtual ~OslDumpPB();

	virtual std::unique_ptr<pesto::pass::IPass> build(PassManagerBuilder &builder, const json &j);
};

} // namespace builder

} // namespace pesto
