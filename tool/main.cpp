/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <cstdio>
#include <filesystem>
#include <iostream>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <clan/clan.h>
#include <osl/osl.h>

#include <pesto/OslScop.hpp>
#include <pesto/Pesto.hpp>

#include "Options.hpp"

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

void print_version(std::ostream &out) { out << "pesto 0.1" << std::endl; }

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
		logger->set_level(spdlog::level::debug);
		logger->debug("Debug mode enabled");
	} else {
		logger->set_level(spdlog::level::info);
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

		if (osl_generic_lookup(scop->extension, OSL_URI_COORDINATES) != NULL) {
			osl_generic_remove(&scop->extension, OSL_URI_COORDINATES);
		}
	}

	fclose(in);
	/* DONE FRONTEND */

	// pesto magic starts here
	// create pesto program
	std::unique_ptr<pesto::Program> program = pesto::Program::Builder()
												  .addScop(scop)
												  .setInputFile(options->inputPath())
												  .setOutputFile(options->outputPath())
												  .build();
	// scop has been copied by builder, we can free memory
	osl_scop_free(scop);

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

		// register all the passes
		// order matter
		passManager->addPass(std::make_unique<pass::ParamContextPass>(10));
		passManager->addPass(std::make_unique<pass::PlutoAutoTransformPass>());
		passManager->addPass(std::make_unique<pass::SplitNestedLoopsPass>());
		passManager->addPass(std::make_unique<pass::PlutoTilingPass>());
		passManager->addPass(std::make_unique<pass::DumpOslPass>("debug."));
		passManager->addPass(std::make_unique<pass::GenClastPass>());
		passManager->addPass(std::make_unique<pass::ClastParamTilingPass>());
		// TODO: should codegen be a pass? Or whole different component ?
		passManager->addPass(std::make_unique<pass::CloogCodegenPass>());
	}

	// run the passes
	logger->info("Running optimizations passes");
	passManager->run(*program.get());

	logger->info("Done");
	return 0;
}
