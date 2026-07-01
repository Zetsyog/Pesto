/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <filesystem>

namespace pesto {

class IByProduct {
  public:
	virtual ~IByProduct() = default;
	virtual std::filesystem::path path() const = 0;
};

} // namespace pesto
