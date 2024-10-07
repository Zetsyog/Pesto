#include <iostream>
#include <pesto/Pesto.hpp>

#include <osl_support.h>

using namespace pesto;

// DumpOslPass

DumpOslPass::DumpOslPass(std::string prefix)
	: path_prefix(prefix), IPestoSingleNestPass("pass-dump-osl") {}

DumpOslPass::~DumpOslPass() {}

void DumpOslPass::runSingleNest(ScopNest &nest) {
	osl_scop_p scop = nest.getScop();
	std::string path =
		path_prefix + "." + std::to_string(nest.getId()) + ".scop";
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