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

class OutputSourceFile : public IByProduct {
  public:
	bool runPreprocessor;

	OutputSourceFile(std::filesystem::path path);
	virtual ~OutputSourceFile() = default;
	virtual std::filesystem::path path() const;

  private:
	std::filesystem::path path_;
};

} // namespace pesto::bundle::byproduct
