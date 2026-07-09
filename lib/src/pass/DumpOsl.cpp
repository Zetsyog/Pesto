/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <iostream>

#include <pesto/osl_support.h>

#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/pass/DumpOsl.hpp>

using namespace pesto::pass;

// DumpOslPass

DumpOslPass::DumpOslPass(std::string prefix) : IPass("dump-osl"), path_prefix(prefix) {}

DumpOslPass::~DumpOslPass() {}

pesto::Bundle DumpOslPass::apply(Bundle &&bundle) {
	auto *scopIR = dynamic_cast<ir::OpenScopIR *>(bundle.get_ir());
	osl_scop_p scop = scopIR->c_scop();
	std::string path = path_prefix + "." + std::to_string(this->dumpCount++) + ".scop";
	logger->info("Dumping scop to {}", path.c_str());
	FILE *fd = fopen(path.c_str(), "w");
	osl_scop_print(fd, scop);
	fclose(fd);

	return std::move(bundle);
}
