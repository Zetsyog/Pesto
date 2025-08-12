#include <iostream>

#include <pesto/osl_support.h>

#include <pesto/pass/OslPass.hpp>

using namespace pesto::pass;
using pesto::pass::metadata::OpenScopMetadata;

// DumpOslPass

DumpOslPass::DumpOslPass(std::string prefix) : IPass("pass-dump-osl"), path_prefix(prefix) {}

DumpOslPass::~DumpOslPass() {}

void DumpOslPass::applyScop(OslScop &nest) {
	osl_scop_p scop = nest.c_scop();
	std::string path = path_prefix + "." + std::to_string(nest.getId()) + ".scop";
	log(std::cerr) << "Dumping scop to " << path << std::endl;
	FILE *fd = fopen(path.c_str(), "w");
	osl_scop_print(fd, scop);
	fclose(fd);
}

OpenScopMetadata::OpenScopMetadata() {}

OpenScopMetadata::~OpenScopMetadata() {
	for (auto i = scops.begin(); i != scops.end(); i++) {
		osl_scop_free(i->second);
	}
}