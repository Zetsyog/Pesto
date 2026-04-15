/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <iostream>

#include <assert.h>
#include <getopt.h>

#include "Options.hpp"

using namespace pesto;

Options::Options() : _configPath(std::nullopt), _readScop(0), _debug(0), _printUsage(0), _printVersion(0) {}

bool Options::readScop() { return this->_readScop != 0; }
std::filesystem::path Options::inputPath() { return this->input; }
std::filesystem::path Options::outputPath() { return this->output; }
std::optional<std::string> Options::configPath() { return this->_configPath; }
bool Options::debug() { return this->_debug != 0; }
bool Options::printUsage() { return this->_printUsage != 0; }
bool Options::printVersion() { return this->_printVersion != 0; }

std::unique_ptr<Options> Options::build(int argc, char **argv) {
	auto opt = std::make_unique<Options>();

	int c;
	while (1) {
		static struct option long_options[] = {{"readscop", no_argument, &opt->_readScop, 1},
											   {"config", required_argument, 0, 'c'},
											   {"output", required_argument, 0, 'o'},
											   {"indent", no_argument, 0, 'i'},
											   {"help", no_argument, 0, 'h'},
											   {"version", no_argument, 0, 'v'},
											   {"debug", no_argument, &opt->_debug, 1},
											   {0, 0, 0, 0}};

		int option_index = 0;

		c = getopt_long(argc, argv, "hivc:o:", long_options, &option_index);

		// detect end of options
		if (c == -1)
			break;

		switch (c) {
		case 0:
			break;
		case 'c':
			opt->_configPath = std::string(optarg);
			break;
		case 'o':
			opt->output = optarg;
			break;
		case 'i':
			break;
		case 'h':
			opt->_printUsage = 1;
			break;
		case 'v':
			opt->_printVersion = 1;
			break;
		case '?':
			// getopt already printed a message
			break;
		default:
			throw std::logic_error("Unexpected option");
		}
	}
	if (optind == argc) {
		throw std::runtime_error("Missing input file");
	}
	// TODO: warning if too many args
	opt->input = std::string(argv[optind]);

	return opt;
}