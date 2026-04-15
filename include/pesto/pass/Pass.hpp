/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#ifndef PESTO_POLYHEDRAL_PASS_H
#define PESTO_POLYHEDRAL_PASS_H

#include <string>

#include <pluto/pluto.h>

#include <pesto/Logging.hpp>
#include <pesto/OslScop.hpp>
#include <pesto/Program.hpp>

namespace pesto {

namespace pass {

/**
 * Base interface for a pass that operates on a Program
 * A pass must have a name
 * A pass must implement the run method that will be called on the Program
 * by the PassManager
 */
class IPass {
  public:
	IPass(std::string name);
	virtual ~IPass();

	/**
	 * @brief apply the pass on the program
	 * @param prog the program to apply the pass on
	 */
	virtual void apply(pesto::Program &prog);

	/**
	 * @brief apply the pass on a single scop
	 * @param nest the nest to apply the pass on
	 */
	virtual void applyScop(pesto::OslScop &nest);

	virtual std::string getName() { return name; }

  protected:
	std::string name;
	std::shared_ptr<spdlog::logger> logger;
};

} // namespace pass

} // namespace pesto

#endif // PESTO_POLYHEDRAL_PASS_H