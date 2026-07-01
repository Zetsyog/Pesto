/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/bundle/byproduct/SideSourceFile.hpp>

using pesto::bundle::byproduct::SideSourceFile;

SideSourceFile::SideSourceFile(std::filesystem::path path) : path_(std::move(path)) {}

std::filesystem::path SideSourceFile::path() const {
	return path_;
}