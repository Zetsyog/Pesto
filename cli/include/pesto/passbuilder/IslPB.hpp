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

class IslPB : public IPassBuilder {
  public:
	static const std::string &getID() {
		static const std::string uuid = "dev-isl";
		return uuid;
	};

	IslPB();
	virtual ~IslPB();

	virtual std::unique_ptr<pesto::pass::IPass> build(PassManagerBuilder &builder, const json &j);
};

} // namespace builder

} // namespace pesto
