/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace pesto {

class Options {
  public:
	static std::unique_ptr<Options> build(int argc, char **argv);

	Options();

	std::filesystem::path inputPath();
	std::filesystem::path outputPath();

	std::optional<std::string> configPath();
	bool readScop();
	bool debug();
	bool printUsage();
	bool printVersion();
	bool indent();

  private:
	std::optional<std::filesystem::path> input_;
	std::optional<std::filesystem::path> output_;
	std::optional<std::string> configPath_;
	int readScop_;
	int debug_;
	int printUsage_;
	int printVersion_;
	int indent_;
};

} // namespace pesto
