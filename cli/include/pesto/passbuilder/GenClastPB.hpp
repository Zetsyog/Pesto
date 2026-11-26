/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <pesto/PassBuilder.hpp>

#include <pesto/pass/clast/GenClast.hpp>

#include <memory>
#include <string>

namespace pesto {

namespace builder {

class GenClastPB : public IPassBuilder {
  public:
	static const std::string &getID() {
		static const std::string uuid = "codegen-clast";
		return uuid;
	};

	GenClastPB();
	virtual ~GenClastPB();

	virtual std::unique_ptr<pesto::pass::IPass> build(PassManagerBuilder &builder, const json &j);

	/**
	 * Applies the options of j on an existing pass. Options absent from j keep
	 * their current value, so passes embedding a GenClastPass can preset it.
	 */
	void configure(pesto::pass::GenClastPass &pass, const json &j);
};

} // namespace builder

} // namespace pesto
