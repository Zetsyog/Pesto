/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#ifndef PESTO_PB_GEN_CLAST_H
#define PESTO_PB_GEN_CLAST_H

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

#endif // PESTO_PB_GEN_CLAST_H