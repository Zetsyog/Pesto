/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/bundle/byproduct/HeaderFile.hpp>

using pesto::bundle::byproduct::HeaderFile;

HeaderFile::HeaderFile(std::filesystem::path path) : path_(std::move(path)) {}

std::filesystem::path HeaderFile::path() const {
	return path_;
}