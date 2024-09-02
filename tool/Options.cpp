#include <assert.h>
#include <getopt.h>
#include <string.h>

#include <fstream>
#include <iostream>

#include "Options.hpp"

PestoOptions::PestoOptions() : readScop(0), configPath(std::nullopt) {}

int PestoOptions::getReadScop() { return this->readScop; }
std::string PestoOptions::getGenPrefix() { return this->genPrefix; }
std::string PestoOptions::getInputPath() { return this->inputPath; }
std::optional<std::string> PestoOptions::getConfigPath() {
	return this->configPath;
}

void print_usage() {
	std::cerr << "Usage: pesto [options] <input file>" << std::endl;
	std::cerr << "Options:" << std::endl;
	std::cerr << "\t--readscop\t\tInput file is openscop format" << std::endl;
	std::cerr << "\t--config <file>\t\tSpecify a JSON configuration file"
			  << std::endl;
	std::cerr << "\t-h, --help\t\tPrint this help message" << std::endl;
	std::cerr << "\t-v, --version\t\tPrint the version number" << std::endl;
}

std::unique_ptr<PestoOptions> PestoOptions::build(int argc, char **argv) {
	auto opt = std::make_unique<PestoOptions>();

	int c;
	while (1) {
		static struct option long_options[] = {
			{"readscop", no_argument, &opt->readScop, 1},
			{"config", required_argument, 0, 0},
			{"help", no_argument, 0, 'h'},
			{"version", no_argument, 0, 'v'},
			{0, 0, 0, 0}};

		int option_index = 0;

		c = getopt_long(argc, argv, "hv", long_options, &option_index);

		// detect end of options
		if (c == -1)
			break;

		switch (c) {
		case 0:
			// if options set a flag, do nothing else
			if (long_options[option_index].flag != 0)
				break;

			if (strcmp(long_options[option_index].name, "config") == 0) {
				opt->configPath = std::string(optarg);
			}
			break;
		case 'h':
			print_usage();
			exit(0);
			break;
		case 'v':
			// TODO: version number
			std::cerr << "pesto 0.1" << std::endl;
			exit(0);
			break;
		case '?':
			// getopt already printed a message
			break;
		default:
			abort();
		}
	}
	if (optind == argc) {
		std::cerr << "[pesto-cli] Failed to parse args. Missing <input_file>"
				  << std::endl;
		print_usage();
		exit(1);
	}
	// TODO: warning if too many args
	opt->inputPath = std::string(argv[optind]);

	/* internal option set by bash wrapper */
	std::ifstream prefixFile(".pesto_prefix", std::ifstream::in);
	if (prefixFile.good()) {
		std::string line;
		std::getline(prefixFile, line);
		opt->genPrefix = line;
	} else {
		opt->genPrefix = ".pesto.gen";
	}
	prefixFile.close();

	return opt;
}