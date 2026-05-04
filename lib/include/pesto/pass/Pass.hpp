/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <set>
#include <string>

#include <pluto/pluto.h>

#include <pesto/Logging.hpp>
#include <pesto/bundle/Bundle.hpp>

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
	 * @brief apply the pass on a Bundle
	 * @param bundle the program to apply the pass on
	 */
	virtual Bundle apply(Bundle &&bundle) = 0;

	/**
	 * @brief get the input IR type
	 * @return the input IR type
	 */
	virtual std::set<std::string> inputIR() const {
		return {};
	}

	/**
	 * @brief get the output IR type
	 * @return the output IR type
	 */
	virtual std::string outputIR() const {
		return "";
	}

	/**
	 * @brief get the name of the pass
	 * Pass name should be unique
	 * It is used for logging and debugging purposes
	 * @return the name of the pass
	 */
	virtual std::string getName() const {
		return name;
	}

  protected:
	std::string name;
	std::shared_ptr<spdlog::logger> logger;
};

} // namespace pass

} // namespace pesto
