/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#ifndef PESTO_PB_CLAST_PARAM_TILING
#define PESTO_PB_CLAST_PARAM_TILING

#include <pesto/passbuilder/PassBuilder.hpp>

#include <memory>

namespace pesto {

namespace builder {

class ClastParamTilingPB : public IPassBuilder {
  public:
	ClastParamTilingPB();
	virtual ~ClastParamTilingPB();

	virtual std::unique_ptr<pesto::pass::IPass> build(PassManagerBuilder &builder, const json &j);
};

} // namespace builder

} // namespace pesto

#endif // PESTO_PB_CLAST_PARAM_TILING