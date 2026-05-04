/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <osl/osl.h>

#include <pesto/Options.hpp>
#include <pesto/Pesto.hpp>

namespace pesto {

class CLI {

  public:
	CLI();

	int start(int argc, char **argv);

	void printUsage(std::ostream &out);
	void printVersion(std::ostream &out);

  private:
	std::string ppCommand_;
	std::string formatCommand_;
	std::shared_ptr<spdlog::logger> logger;

	/**
	 * @brief Parse command line options.
	 * @param argc Number of command line arguments.
	 * @param argv Command line arguments.
	 * @return Unique pointer to parsed options.
	 */
	std::unique_ptr<Options> parseOptions(int argc, char **argv);

	/* FRONTEND */
	/**
	 * @brief Parse an openscop file.
	 * @param inputPath Path to the input file.
	 * @return Unique pointer to the parsed scop.
	 */
	osl_scop_p parseScopFile(const std::filesystem::path &inputPath);
	/**
	 * @brief Parse a C source file.
	 * Uses clan to extract the scop from the source file.
	 * @param inputPath Path to the input file.
	 * @return Unique pointer to the parsed scop.
	 */
	osl_scop_p parseCSourceFile(const std::filesystem::path &inputPath);

	/**
	 * @brief Get the coordinates from a scop.
	 * Coordinates holds information about where the scop pragmas were located in the original source file, which is
	 * useful to know where to place the generated code.
	 * @param scop The scop to extract coordinates from.
	 * @return Unique pointer to the coordinates.
	 */
	osl_coordinates_p getCoordinates(osl_scop_p &scop);
	/* END FRONTEND */

	std::optional<std::filesystem::path> getBundleOutputSourceFile(const Bundle &bundle);

	void writePreprocessorInputFile(const pesto::Bundle &bundle, const std::filesystem::path &target);
	void runPreprocessor(const std::filesystem::path &inputFile, const std::filesystem::path &outputFile);
	void sandwichCode(const std::filesystem::path &original, const std::filesystem::path &dest,
					  const std::filesystem::path &generatedCodeFile, osl_coordinates_p coord, const Bundle &bundle);

	void runFormatter(const std::filesystem::path &targetFile);
};

} // namespace pesto