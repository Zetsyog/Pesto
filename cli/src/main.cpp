/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <clan/clan.h>
#include <osl/osl.h>

#include <pesto/Pesto.hpp>

#include <pesto/Options.hpp>
#include <pesto/PassBuilder.hpp>
#include <pesto/utils/Process.hpp>

void print_usage(std::ostream &out) {
	out << "Usage: pesto [options] <input file>" << std::endl;
	out << "Options:" << std::endl;
	out << "\t--readscop\t\tInput file is openscop format" << std::endl;
	out << "\t--config,-c <file>\t\tSpecify a JSON configuration file" << std::endl;
	out << "\t--output,-o <file>\t\tSet output file path" << std::endl;
	out << "\t--indent,-i\t\tFormat generated code" << std::endl;
	out << "\t--debug\t\t\tEnable debug mode (more verbose output, keep intermediate files...)" << std::endl;
	out << "\t--help,-h\t\tPrint this help message" << std::endl;
	out << "\t--version,-v\t\tPrint the version number" << std::endl;
}

void print_version(std::ostream &out) {
	out << "Pesto version " << PESTO_VERSION_STRING << std::endl;
}

std::optional<std::filesystem::path> getOutputSourceFile(const pesto::Bundle &bundle) {
	for (auto &byproduct : bundle.byproducts()) {
		if (std::type_index(typeid(*byproduct)) ==
			std::type_index(typeid(pesto::bundle::byproduct::OutputSourceFile))) {
			auto &outputProduct = dynamic_cast<pesto::bundle::byproduct::OutputSourceFile &>(*byproduct);
			return outputProduct.path();
		}
	}
	return std::nullopt;
}

int main(int argc, char **argv) {
	auto logger = pesto::log::get_logger("pesto-cli");

	logger->info("Starting pesto-cli...");

	std::unique_ptr<pesto::Options> options;
	try {
		options = pesto::Options::build(argc, argv);
	} catch (const std::exception &e) {
		logger->error("Error while parsing options: {}", e.what());
		return 1;
	}

	if (options->printUsage()) {
		print_usage(std::cerr);
		return 0;
	}

	if (options->printVersion()) {
		print_version(std::cerr);
		return 0;
	}

	if (options->debug()) {
		spdlog::set_level(spdlog::level::debug);
		pesto::log::set_level(spdlog::level::debug);
		logger->debug("Debug mode enabled");
	} else {
		pesto::log::set_level(spdlog::level::info);
	}

	/* FRONTEND */
	// check if input file is provided
	auto inputPathStr = options->inputPath().string();
	logger->info("Received input file '{}'", inputPathStr.c_str());

	// check if input file is regular file, else shout
	struct stat path_stat;
	stat(options->inputPath().c_str(), &path_stat);
	if (!S_ISREG(path_stat.st_mode)) {
		logger->error("Input file is not a regular file");
		return 1;
	}

	// open input file
	FILE *in = fopen(options->inputPath().c_str(), "r");
	if (in == NULL) {
		logger->error("Error while opening input file '{}'", inputPathStr.c_str());
		return 1;
	}

	osl_scop_p scop;
	osl_coordinates_p coord_info = nullptr;

	// if input is scop file
	if (options->readScop()) {
		logger->info("Considering input as openscop file");
		scop = osl_scop_read(in);
		if (scop == NULL) {
			logger->error("Error parsing scop file");
			return 1;
		}
	} else { // else we consider it is plain c and extract openscop using clan
		logger->info("Considering input file as plain c");
		logger->info("Running clan on '{}'", inputPathStr.c_str());
		clan_options_p clan_opt = clan_options_malloc();
		scop = clan_scop_extract(in, clan_opt);
		clan_options_free(clan_opt);
		if (scop == NULL) {
			logger->error("Clan scop extraction failed. Abort");
			return 1;
		}
		if (osl_generic_lookup(scop->extension, OSL_URI_COORDINATES) == NULL) {
			logger->error("Scop does not contain coordinates, cannot run codegen");
			return 1;
		}
		coord_info = osl_coordinates_clone((osl_coordinates_p)osl_generic_lookup(scop->extension, OSL_URI_COORDINATES));
	}

	fclose(in);
	/* DONE FRONTEND */

	// scop has been copied by builder, we can free memory
	pesto::Bundle bundle(std::make_unique<pesto::ir::OpenScopIR>(scop));
	auto &fileInfo = bundle.metadata.get<pesto::bundle::metadata::FileInfo>();
	fileInfo.input = options->inputPath();
	fileInfo.output = options->outputPath();

	std::unique_ptr<pesto::PassManager> passManager;

	if (options->configPath().has_value()) {
		std::unique_ptr<pesto::builder::PassManagerBuilder> builder =
			pesto::builder::PassManagerBuilder::createDefault();

		passManager = builder->build(options->configPath().value());
		if (passManager == nullptr) {
			logger->error("Error while building pass manager");
			return 1;
		}
	} else {
		using namespace pesto;

		passManager = std::make_unique<pesto::PassManager>();

		// by default, we run pluto and then generate code with cloog
		passManager->addPass(std::make_unique<pass::PlutoTilingPass>());
		passManager->addPass(std::make_unique<pass::CloogCodegenPass>());
	}

	// run the passes
	logger->info("Running passes");
	bundle = passManager->run(std::move(bundle));

	if (!bundle.empty()) {
		logger->warn("Passmanager did not consume the bundle, did you forget to add a codegen pass at the end of your "
					 "pass pipeline ?");
		logger->info("Bundle output IR is {}", bundle.get_ir()->id().c_str());
	}

	std::string ppCommand = "gcc";

	std::optional<std::filesystem::path> bundleOutputFile = getOutputSourceFile(bundle);
	if (!bundleOutputFile.has_value()) {
		logger->warn("No output source file generated by the bundle, skipping preprocessing step");
		logger->info("Done");
		return 0;
	}
	logger->info("Generated output file: {}", bundleOutputFile.value().string().c_str());

	auto &macro = bundle.metadata.get<pesto::bundle::metadata::Macro>();
	std::filesystem::path bundleOutputWithMacros = bundleOutputFile.value().string() + ".input.c";
	{
		std::ofstream out(bundleOutputWithMacros);
		// Write statements macros
		for (const auto &macroLine : macro.preprocessedList) {
			out << macroLine << std::endl;
		}

		// Write variable declarations at the top of the file
		auto &varDecl = bundle.metadata.get<pesto::bundle::metadata::VarDecl>();
		out << "/* Variable declarations */" << std::endl;
		// TODO: group variable declaration
		for (auto &varId : varDecl.vars()) {
			out << varDecl.typeName << " " << varId << ";" << std::endl;
		}

		// copy rest of the bundle output file content
		std::ifstream bundleOutput(bundleOutputFile.value());
		std::string line;
		while (std::getline(bundleOutput, line)) {
			out << line << std::endl;
		}

		out.close();
		logger->info("Generated file with macros: {}", bundleOutputWithMacros.string().c_str());
	}

	logger->info("Running preprocessor on file {}", bundleOutputWithMacros.string().c_str());
	auto processedFile = std::filesystem::path(bundleOutputFile.value().string() + ".pre");

	using namespace pesto::utils;
	auto result = Process{}
					  .command({ppCommand, "-E", "-P", bundleOutputWithMacros.string()})
					  .stdout_to(process::RedirectTo::file(processedFile.string()))
					  .stderr_to(process::RedirectTo::null())
					  .run();

	if (result.exit_code != 0) {
		logger->error("Preprocessing failed: exit code {}", result.exit_code);
		return 1;
	}

	// copy input file to output file until reached line_start, then append preprocessed file content until line_end,
	// then copy rest of input file
	std::ifstream inputFile(options->inputPath());
	std::ofstream outputFile(options->outputPath());

	for (auto &byproduct : bundle.byproducts()) {
		if (std::type_index(typeid(*byproduct)) == std::type_index(typeid(pesto::bundle::byproduct::HeaderFile))) {
			auto &headerProduct = dynamic_cast<pesto::bundle::byproduct::HeaderFile &>(*byproduct);
			outputFile << "#include \"" << headerProduct.path().filename().string() << "\"" << std::endl;
		}
	}

	for (auto &macroLine : macro.nonProcessedList) {
		outputFile << macroLine << std::endl;
	}

	std::string line;
	int currentLine = 1;
	while (std::getline(inputFile, line)) {
		if (currentLine < coord_info->line_start - 1) {
			outputFile << line << std::endl;
		} else if (currentLine == coord_info->line_start) {
			// append preprocessed file content with proper indentation
			std::ifstream preprocessedFile(processedFile);
			std::string preLine;
			while (std::getline(preprocessedFile, preLine)) {
				outputFile << std::string(coord_info->indent, ' ') << preLine << std::endl;
			}
			preprocessedFile.close();
		} else if (currentLine > coord_info->line_end) {
			outputFile << line << std::endl;
		}
		currentLine++;
	}

	inputFile.close();
	outputFile.close();

	logger->debug("Removing file: {} ({})", bundleOutputFile.value().string().c_str(),
				  std::filesystem::remove(bundleOutputFile.value()));
	logger->debug("Removing file: {} ({})", processedFile.string().c_str(), std::filesystem::remove(processedFile));

	osl_coordinates_free(coord_info);

	if (options->indent()) {
		std::string indentCommand = "clang-format";
		auto indentResult = Process{}
								.command({indentCommand, options->outputPath().string()})
								.stdout_to(process::RedirectTo::null())
								.stderr_to(process::RedirectTo::null())
								.run();

		if (indentResult.exit_code != 0) {
			logger->warn("Code formatting failed: exit code {}", indentResult.exit_code);
		}
	}

	logger->info("Done");
	return 0;
}
