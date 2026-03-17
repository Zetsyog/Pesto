/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#ifndef PESTO_OPENSCOP_OPTIMIZER_H
#define PESTO_OPENSCOP_OPTIMIZER_H

#include <memory>
#include <vector>

#include <osl/osl.h>

#include <pesto/OslScop.hpp>
#include <pesto/Program.hpp>
#include <pesto/pass/Pass.hpp>

namespace pesto {

/**
 * @brief PassManager class
 * Used to run different optimization passes on a polyhedral code
 * Take as input a polyhedral program in OpenScop format
 * Two kinds of passes can be run:
 * - Polyhedral passes
 * - AST passes
 * Then c code can be generated
 *
 */
class PassManager {
  public:
	PassManager();
	virtual ~PassManager();

	/**
	 * Run optimization passes
	 * Multiple passes can be run
	 */
	void run(Program &prog);

	void addPass(std::unique_ptr<pesto::pass::IPass> pass);

  private:
	std::vector<std::unique_ptr<pesto::pass::IPass>> passList;
};

} // namespace pesto

#endif // PESTO_OPENSCOP_OPTIMIZER_H