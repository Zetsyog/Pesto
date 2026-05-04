/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <cloog/cloog.h>
#include <pesto/pass/Pass.hpp>

namespace pesto {

namespace pass {

/**
 * A pass that will alter the clast
 * It will search for clast_for that are marked for vectorization
 * and will add the appropriate pragmas
 */
class CustomVecPragma : public IPass {
  private:
	std::string pragma;

  public:
	CustomVecPragma();
	virtual ~CustomVecPragma();

	virtual Bundle apply(Bundle &&bundle) override;

	std::set<std::string> inputIR() const override;
	std::string outputIR() const override;

	/**
	 * @brief set the pragma to add to the clast
	 */
	void setPragma(std::string pragma);
};

} // namespace pass

} // namespace pesto
