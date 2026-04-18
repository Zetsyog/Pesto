/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <filesystem>

#include <pesto/bundle/ByProduct.hpp>

namespace pesto::bundle::byproduct {

class SideSourceFile : public IByProduct {
  public:
	bool runPreprocessor;

	SideSourceFile(std::filesystem::path path);
	virtual ~SideSourceFile() = default;
	virtual std::filesystem::path path() const;

  private:
	std::filesystem::path path_;
};

} // namespace pesto::bundle::byproduct
