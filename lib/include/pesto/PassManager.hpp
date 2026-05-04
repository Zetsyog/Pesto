/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <memory>

#include <spdlog/spdlog.h>

#include <osl/osl.h>

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
	Bundle run(Bundle &&bundle);

	void addPass(std::unique_ptr<pesto::pass::IPass> pass);

  private:
	std::shared_ptr<spdlog::logger> logger;
	std::vector<std::unique_ptr<pesto::pass::IPass>> passList;
};

} // namespace pesto
