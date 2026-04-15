/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#ifndef PESTO_OPTIONS_H
#define PESTO_OPTIONS_H

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

  private:
	std::filesystem::path input;
	std::filesystem::path output;
	std::optional<std::string> _configPath;
	int _readScop;
	int _debug;
	int _printUsage;
	int _printVersion;
};

} // namespace pesto

#endif // PESTO_OPTIONS_H