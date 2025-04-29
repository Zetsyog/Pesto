#include <iostream>

#include <assert.h>
#include <getopt.h>

#include "Options.hpp"

using namespace pesto;

Options::Options() : configPath(std::nullopt), readScop(0) {}

int Options::getReadScop() { return this->readScop; }
std::filesystem::path Options::getInputPath() { return this->input; }
std::filesystem::path Options::getOutputPath() { return this->output; }
std::optional<std::string> Options::getConfigPath() { return this->configPath; }

void print_usage() {
	std::cerr << "Usage: pesto [options] <input file>" << std::endl;
	std::cerr << "Options:" << std::endl;
	std::cerr << "\t--readscop\t\tInput file is openscop format" << std::endl;
	std::cerr << "\t--config,-c <file>\t\tSpecify a JSON configuration file"
			  << std::endl;
	std::cerr << "\t--output,-o <file>\t\tSpecify a JSON configuration file"
			  << std::endl;
	std::cerr << "\t--indent,-i\t\tFormat generated code" << std::endl;
	std::cerr << "\t--help,-h\t\tPrint this help message" << std::endl;
	std::cerr << "\t--version,-v\t\tPrint the version number" << std::endl;
}

std::unique_ptr<Options> Options::build(int argc, char **argv) {
	auto opt = std::make_unique<Options>();

	int c;
	while (1) {
		static struct option long_options[] = {
			{"readscop", no_argument, &opt->readScop, 1},
			{"config", required_argument, 0, 'c'},
			{"output", required_argument, 0, 'o'},
			{"indent", no_argument, 0, 'i'},
			{"help", no_argument, 0, 'h'},
			{"version", no_argument, 0, 'v'},
			{0, 0, 0, 0}};

		int option_index = 0;

		c = getopt_long(argc, argv, "hvc:o:", long_options, &option_index);

		// detect end of options
		if (c == -1)
			break;

		switch (c) {
		case 0:
			break;
		case 'c':
			opt->configPath = std::string(optarg);
			break;
		case 'o':
			opt->output = optarg;
			break;
		case 'i':
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
	opt->input = std::string(argv[optind]);

	return opt;
}