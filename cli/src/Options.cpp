/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <iostream>

#include <assert.h>
#include <getopt.h>

#include <pesto/Options.hpp>

using namespace pesto;

Options::Options() : configPath_(std::nullopt), readScop_(0), debug_(0), printUsage_(0), printVersion_(0), indent_(0) {}

bool Options::readScop() {
	return this->readScop_ != 0;
}
std::filesystem::path Options::inputPath() {
	// input path is required, should have been checked in build()
	if (!input_.has_value()) {
		throw std::runtime_error("Input path is not set");
	}
	return this->input_.value();
}
std::filesystem::path Options::outputPath() {
	// output path is optional, if not set it will be derived from input path
	// this case should not exist at this point, as output path is set to a default value in build() if not provided by
	// user
	if (!output_.has_value()) {
		throw std::runtime_error("Output path is not set");
	}
	return this->output_.value();
}
std::optional<std::string> Options::configPath() {
	return this->configPath_;
}
bool Options::debug() {
	return this->debug_ != 0;
}
bool Options::printUsage() {
	return this->printUsage_ != 0;
}
bool Options::printVersion() {
	return this->printVersion_ != 0;
}

bool Options::indent() {
	return this->indent_ != 0;
}

bool Options::isValid() {
	if (this->input_.has_value()) {
		return true;
	}

	if (this->printUsage() || this->printVersion()) {
		return true;
	}

	return false;
}

std::unique_ptr<Options> Options::build(int argc, char **argv) {
	auto opt = std::make_unique<Options>();

	int c;
	while (1) {
		static struct option long_options[] = {{"readscop", no_argument, &opt->readScop_, 1},
											   {"config", required_argument, 0, 'c'},
											   {"output", required_argument, 0, 'o'},
											   {"indent", no_argument, 0, 'i'},
											   {"help", no_argument, 0, 'h'},
											   {"version", no_argument, 0, 'v'},
											   {"debug", no_argument, &opt->debug_, 1},
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
			opt->configPath_ = std::string(optarg);
			break;
		case 'o':
			opt->output_ = optarg;
			break;
		case 'i':
			opt->indent_ = 1;
			break;
		case 'h':
			opt->printUsage_ = 1;
			break;
		case 'v':
			opt->printVersion_ = 1;
			break;
		case '?':
			// getopt already printed a message
			break;
		default:
			throw std::logic_error("Unexpected option");
		}
	}
	if (optind != argc) {
		opt->input_ = argv[optind];
	}

	if (opt->output_ == std::nullopt) {
		opt->output_ = opt->input_->stem().string() + ".pesto.c";
	}

	return opt;
}