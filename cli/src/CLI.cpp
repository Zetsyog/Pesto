/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <clan/clan.h>
#include <osl/osl.h>

#include <pesto/CLI.hpp>
#include <pesto/Options.hpp>
#include <pesto/PassBuilder.hpp>
#include <pesto/Pesto.hpp>
#include <pesto/utils/Process.hpp>
#include <pesto/utils/TemporaryDirectory.hpp>

#include <cstring>
#include <fstream>

using pesto::CLI;

CLI::CLI() : ppCommand_("gcc"), formatCommand_("clang-format") {
	logger = pesto::log::get_logger("pesto-cli");
}

void CLI::printUsage(std::ostream &out) {
	out << "Usage: pesto [options] <input file>" << std::endl;
	out << "Options:" << std::endl;
	out << "\t"
		<< "--readscop"
		<< "\t\t"
		<< "Input file is openscop format" << std::endl;
	out << "\t"
		<< "--config,-c <file>"
		<< "\t\t"
		<< "Specify a JSON configuration file" << std::endl;
	out << "\t"
		<< "--output,-o <file>"
		<< "\t\t"
		<< "Set output file path" << std::endl;
	out << "\t"
		<< "--indent,-i"
		<< "\t\t"
		<< "Format generated code" << std::endl;
	out << "\t"
		<< "--debug"
		<< "\t\t\t"
		<< "Enable debug mode (more verbose output, keep intermediate files...)" << std::endl;
	out << "\t"
		<< "--help,-h"
		<< "\t\t"
		<< "Print this help message" << std::endl;
	out << "\t"
		<< "--version,-v"
		<< "\t\t"
		<< "Print the version number" << std::endl;
}

void CLI::printVersion(std::ostream &out) {
	out << "Pesto version " << PESTO_VERSION_STRING << std::endl;
}

int CLI::start(int argc, char **argv) try {
	logger->info("Starting pesto-cli...");

	auto options = parseOptions(argc, argv);

	if (options->debug()) {
		pesto::log::set_level(spdlog::level::debug);
		spdlog::set_level(spdlog::level::debug);
		logger->debug("Debug mode enabled");
	} else {
		pesto::log::set_level(spdlog::level::info);
	}

	if (!options->isValid()) {
		logger->error("Invalid options");
		printUsage(std::cerr);
		return 1;
	}

	if (options->printUsage()) {
		printUsage(std::cerr);
		return 0;
	}

	if (options->printVersion()) {
		printVersion(std::cerr);
		return 0;
	}

	std::filesystem::path inputFile = options->inputPath();

	/* Frontend */
	logger->info("Received input file '{}'", inputFile.string().c_str());

	/* Check if input file is valid*/
	if (!std::filesystem::exists(inputFile)) {
		logger->error("Input file '{}' does not exist", inputFile.string().c_str());
		return 1;
	}
	if (!std::filesystem::is_regular_file(inputFile)) {
		logger->error("Input file '{}' is not a regular file", inputFile.string().c_str());
		return 1;
	}
	if (std::filesystem::exists(options->outputPath()) &&
		std::filesystem::equivalent(inputFile, options->outputPath())) {
		throw std::invalid_argument("Input and output must be different files");
	}

	osl_scop_p scop = nullptr;
	if (options->readScop()) {
		logger->info("Considering input as openscop file");
		scop = parseScopFile(inputFile);
	} else {
		logger->info("Considering input file as plain c");
		scop = parseCSourceFile(inputFile);
	}

	if (scop == nullptr) {
		logger->error("Failed to parse input file '{}'", inputFile.string().c_str());
		return 1;
	}
	pesto::Bundle bundle(std::make_unique<pesto::ir::OpenScopIR>(scop));
	if (scop->next) {
		throw std::invalid_argument("Only one scop region per input file is supported");
	}
	if (!scop->statement) {
		throw std::invalid_argument("The scop region contains no statements to transform");
	}

	/* retrieve scop coordinates so we know where in the output file we should place the generated code */
	std::unique_ptr<osl_coordinates, decltype(&osl_coordinates_free)> coord_info(getCoordinates(scop),
																				 osl_coordinates_free);
	if (coord_info == nullptr) {
		logger->warn("Scop does not contain coordinates, cannot run codegen");
	}
	/* Done frontend */

	/* Polyhedral magic will go here */

	/* add metadata that holds input and output paths */
	/* this can be useful for some passes like code generation */
	auto &fileInfo = bundle.metadata.get<pesto::bundle::metadata::FileInfo>();
	fileInfo.input = options->inputPath();
	fileInfo.output = options->outputPath();

	/* Build pass manager */
	std::unique_ptr<pesto::PassManager> passManager;

	/* if json config path is specified */
	if (options->configPath().has_value()) {
		logger->info("Building pass manager from config file '{}'", options->configPath().value().c_str());

		std::unique_ptr<pesto::builder::PassManagerBuilder> builder =
			pesto::builder::PassManagerBuilder::createDefault();

		passManager = builder->build(options->configPath().value());
		if (passManager == nullptr) {
			logger->error("Error while building pass manager");
			return 1;
		}
	} else {
		logger->info("No config file specified, using default pass pipeline");

		passManager = std::make_unique<pesto::PassManager>();

		// by default, we run pluto and then generate code with cloog
		passManager->addPass(std::make_unique<pass::PlutoPass>());
		passManager->addPass(std::make_unique<pass::CloogCodegenPass>());
	}

	logger->info("Applying passes");
	bundle = passManager->run(std::move(bundle));

	if (!bundle.empty()) {
		logger->warn("PassManager did not consume the bundle, did you forget to add a codegen pass at the end of your "
					 "pass pipeline ?");
		logger->warn("Bundle output IR is {}", bundle.get_ir()->id().c_str());
	}

	auto bundleOutputFile = getBundleOutputSourceFile(bundle);
	if (!bundleOutputFile.has_value()) {
		logger->warn("No output source file generated by the bundle, no more work to do");
		logger->info("Done");
		return 0;
	}

	logger->info("Generated output file: {}", bundleOutputFile.value().string().c_str());
	if (options->readScop() || !coord_info) {
		throw std::runtime_error("Source code generation requires C input with scop coordinates");
	}

	/* list of files that must be cleaned up */
	std::set<std::filesystem::path> garbageFiles;

	std::filesystem::path processingFile = bundleOutputFile.value();
	garbageFiles.insert(processingFile);

	auto &macro = bundle.metadata.get<pesto::bundle::metadata::Macro>();
	if (!macro.preprocessedList.empty()) {
		std::filesystem::path ppFileInput = processingFile.parent_path() / (processingFile.stem().string() + ".in.c");
		logger->info("Writing preprocessor input file '{}'", ppFileInput.string().c_str());
		writePreprocessorInputFile(bundle, ppFileInput);
		garbageFiles.insert(ppFileInput);

		logger->info("Running preprocessor on file '{}'", ppFileInput.string().c_str());
		std::filesystem::path ppFileOutput = processingFile.parent_path() / (processingFile.stem().string() + ".pre");
		runPreprocessor(ppFileInput, ppFileOutput);
		garbageFiles.insert(ppFileOutput);

		processingFile = ppFileOutput;
	}

	/* Final work: sandwich generated code
		First copy input file until scop region
		Then append code generated by pesto
		Then copy rest of input file
	*/
	sandwichCode(options->inputPath(), options->outputPath(), processingFile, coord_info.get(), bundle);

	if (options->indent()) {
		logger->info("Formatting generated code with '{}'", formatCommand_.c_str());
		runFormatter(options->outputPath());
	}

	/* clean up */
	for (const auto &file : garbageFiles) {
		if (std::filesystem::exists(file)) {
			logger->debug("Removing file: {} ({})", file.string().c_str(), std::filesystem::remove(file));
		}
	}

	return 0;
} catch (const std::exception &e) {
	logger->error("{}", e.what());
	return 1;
}

std::unique_ptr<pesto::Options> CLI::parseOptions(int argc, char **argv) {
	return Options::build(argc, argv);
}

osl_scop_p CLI::parseScopFile(const std::filesystem::path &inputFile) {
	osl_scop_p scop = nullptr;
	FILE *in = fopen(inputFile.c_str(), "r");
	if (in == nullptr) {
		logger->error("Error while opening input file '{}'", inputFile.string().c_str());
		return nullptr;
	}
	scop = osl_scop_read(in);

	fclose(in);
	return scop;
}

osl_scop_p CLI::parseCSourceFile(const std::filesystem::path &inputFile) {
	osl_scop_p scop = nullptr;
	FILE *in = fopen(inputFile.c_str(), "r");
	if (in == nullptr) {
		logger->error("Error while opening input file '{}'", inputFile.string().c_str());
		return nullptr;
	}

	logger->info("Running clan on '{}'", inputFile.string().c_str());

	clan_options_p clan_options = clan_options_malloc();
	clan_options->name = strdup(inputFile.c_str());

	scop = clan_scop_extract(in, clan_options);
	clan_options_free(clan_options);
	fclose(in);
	if (scop == nullptr) {
		logger->error("Clan scop extraction failed.");
		return nullptr;
	}

	return scop;
}

osl_coordinates_p CLI::getCoordinates(osl_scop_p &scop) {
	if (osl_generic_lookup(scop->extension, OSL_URI_COORDINATES) == nullptr) {
		return nullptr;
	}
	return osl_coordinates_clone((osl_coordinates_p)osl_generic_lookup(scop->extension, OSL_URI_COORDINATES));
}

std::optional<std::filesystem::path> CLI::getBundleOutputSourceFile(const Bundle &bundle) {
	for (auto &byproduct : bundle.byproducts()) {
		auto &product = *byproduct;
		if (std::type_index(typeid(product)) == std::type_index(typeid(pesto::bundle::byproduct::OutputSourceFile))) {
			auto &sourceProduct = dynamic_cast<pesto::bundle::byproduct::OutputSourceFile &>(product);
			return sourceProduct.path();
		}
	}
	return std::nullopt;
}

void CLI::writePreprocessorInputFile(const pesto::Bundle &bundle, const std::filesystem::path &target) {
	std::ofstream out(target);
	if (!out.is_open()) {
		throw std::runtime_error("Could not open preprocessor input for writing: " + target.string());
	}

	/* write macros that must be preprocessed */
	/* ususally statements macros */
	auto &macro = bundle.metadata.get<pesto::bundle::metadata::Macro>();
	for (auto &macroLine : macro.preprocessedList) {
		out << macroLine << std::endl;
	}

	/* write variable declarations */
	auto &varDecl = bundle.metadata.get<pesto::bundle::metadata::VarDecl>();
	out << "/* Variable declarations */" << std::endl;
	for (auto &varId : varDecl.vars()) {
		out << varDecl.typeName << " " << varId << ";" << std::endl;
	}

	/* copy rest of the bundle output file content */
	auto bundleOutputFile = getBundleOutputSourceFile(bundle);
	if (!bundleOutputFile.has_value()) {
		throw std::runtime_error("No generated source to preprocess");
	}

	std::ifstream bundleOutput(bundleOutputFile.value());
	if (!bundleOutput.is_open()) {
		throw std::runtime_error("Could not read generated source: " + bundleOutputFile->string());
	}
	std::string line;
	while (std::getline(bundleOutput, line)) {
		out << line << std::endl;
	}

	out.close();
	if (bundleOutput.bad() || !out) {
		throw std::runtime_error("Failed to write preprocessor input: " + target.string());
	}
}

void CLI::runPreprocessor(const std::filesystem::path &input, const std::filesystem::path &output) {
	using namespace pesto::utils;
	std::vector<std::string> cmd = {ppCommand_, "-E", "-P", input.string()};
	auto result = Process{}
					  .command(cmd)
					  .stdout_to(process::RedirectTo::file(output.string()))
					  .stderr_to(process::RedirectTo::inherit())
					  .run();

	if (result.exit_code != 0) {
		logger->error("Preprocessing failed: exit code {}", result.exit_code);
		logger->error("Command: {}", fmt::join(cmd, " "));
		throw std::runtime_error("Generated code preprocessing failed");
	}
}

void CLI::sandwichCode(const std::filesystem::path &input, const std::filesystem::path &output,
					   const std::filesystem::path &generatedCode, osl_coordinates_p coord, const Bundle &bundle) {
	if (!coord || coord->line_start < 1 || coord->line_end < coord->line_start || coord->indent < 0) {
		throw std::runtime_error("Invalid scop source coordinates");
	}
	std::ifstream inputFile(input);
	std::ifstream generatedFile(generatedCode);

	if (!inputFile.is_open()) {
		throw std::runtime_error("Could not read input file: " + input.string());
	}
	if (!generatedFile.is_open()) {
		throw std::runtime_error("Could not read generated code: " + generatedCode.string());
	}
	// Stage beside the destination so publishing is one rename on the same filesystem.
	utils::TemporaryDirectory staging(output.has_parent_path() ? output.parent_path() : ".");
	const auto stagedOutput = staging.path() / output.filename();
	std::ofstream outputFile(stagedOutput);
	if (!outputFile.is_open()) {
		throw std::runtime_error("Could not open output for writing: " + output.string());
	}
	outputFile.exceptions(std::ios::badbit | std::ios::failbit);

	/* write header includes if any */
	for (auto &byproduct : bundle.byproducts()) {
		auto &product = *byproduct;
		if (std::type_index(typeid(product)) == std::type_index(typeid(pesto::bundle::byproduct::HeaderFile))) {
			auto &headerProduct = dynamic_cast<pesto::bundle::byproduct::HeaderFile &>(product);
			outputFile << "#include \"" << headerProduct.path().filename().string() << "\"" << std::endl;
		}
	}

	/* write macros that are not preprocessed */
	auto &macro = bundle.metadata.get<pesto::bundle::metadata::Macro>();
	for (auto &macroLine : macro.nonProcessedList) {
		outputFile << macroLine << std::endl;
	}

	/* copy original file until reaching scop section */
	std::string line;
	long int currentLine = 1;
	while (std::getline(inputFile, line)) {
		if (currentLine == coord->line_start) {
			// Generated iterators must not redeclare variables in the surrounding function.
			outputFile << std::string(coord->indent, ' ') << "{" << std::endl;
			std::string genLine;
			while (std::getline(generatedFile, genLine)) {
				outputFile << std::string(coord->indent + 4, ' ') << genLine << std::endl;
			}
			outputFile << std::string(coord->indent, ' ') << "}" << std::endl;
		}
		if (currentLine > coord->line_end || currentLine < coord->line_start) {
			outputFile << line << std::endl;
		}
		currentLine++;
	}

	if (inputFile.bad() || generatedFile.bad() || currentLine <= coord->line_end) {
		throw std::runtime_error("Could not assemble output from the scop source coordinates");
	}
	outputFile.close();
	std::filesystem::rename(stagedOutput, output);
}

void CLI::runFormatter(const std::filesystem::path &target) {
	using namespace pesto::utils;
	std::vector<std::string> cmd = {formatCommand_, "-i", target.string()};
	process::Result result =
		Process{}.command(cmd).stdout_to(process::RedirectTo::null()).stderr_to(process::RedirectTo::null()).run();

	logger->debug("Running formatter command: {}", fmt::join(cmd, " "));

	if (result.exit_code != 0) {
		logger->warn("Code formatting failed: exit code {}", result.exit_code);
		logger->warn("Command: {}", fmt::join(cmd, " "));
	}
}
