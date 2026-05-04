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

	/**
	 * @brief Write the preprocessor input file.
	 * This file contains the generated code from the codegen pass, as well as any necessary includes and macros. This
	 * file destined to be passed to the preprocessor
	 * @param bundle The bundle containing the code to preprocess.
	 * @param target The path to the target file.
	 */
	void writePreprocessorInputFile(const pesto::Bundle &bundle, const std::filesystem::path &target);

	/**
	 * @brief Run the preprocessor on a file.
	 * @param inputFile The path to the input file.
	 * @param outputFile The path to the output file.
	 */
	void runPreprocessor(const std::filesystem::path &inputFile, const std::filesystem::path &outputFile);
	/**
	 * @brief Sandwich the generated code between two parts of the original file.
	 * This is done by copying the original file until reaching the scop section, then inserting the pesto generated
	 * code, and finally copying the rest of the original file. This allows to preserve any code outside of the scop, as
	 * well as any formatting and comments in the original file. The generated code is expected to be preprocessed, so
	 * that it can be directly inserted in the original file without any further processing. The generated code is
	 * expected to be located in a separate file, which is passed as an argument to this function, so that it can be
	 * read and inserted in the original file. The coordinates of the scop are also passed as an argument, so that we
	 * know where in the original file we should insert the generated code.
	 * @param original The path to the original file.
	 * @param dest The path to the destination file.
	 * @param generatedCodeFile The path to the generated code file.
	 * @param coord The coordinates of the scop.
	 * @param bundle The bundle containing the code to generate.
	 */
	void sandwichCode(const std::filesystem::path &original, const std::filesystem::path &dest,
					  const std::filesystem::path &generatedCodeFile, osl_coordinates_p coord, const Bundle &bundle);

	/**
	 * @brief Run the code formatter on a file.
	 * @param targetFile The path to the target file.
	 */
	void runFormatter(const std::filesystem::path &targetFile);
};

} // namespace pesto