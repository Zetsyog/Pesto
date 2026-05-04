/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <filesystem>

namespace pesto::bundle::metadata {
struct FileInfo {
	std::filesystem::path input;
	std::filesystem::path output;
};
} // namespace pesto::bundle::metadata