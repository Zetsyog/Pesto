/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <iostream>
#include <string>
#include <vector>

#include <pesto/osl_support.h>
#include <pesto/pass/OslPass.hpp>

using namespace pesto::pass;

SplitNestedLoopsPass::SplitNestedLoopsPass() : IPass("split-nest") {}

SplitNestedLoopsPass::~SplitNestedLoopsPass() {}

void SplitNestedLoopsPass::apply(Program &prog) {
	std::vector<OslScop *> original_nests = prog.getScopNests();

	std::vector<OslScop *> new_nests;
	int nextNestId = 0;

	// TODO: better algorithm ?
	for (auto it = original_nests.begin(); it != original_nests.end(); ++it) {
		auto &nest = *it;
		auto distance = std::distance(original_nests.begin(), it);

		this->logger->debug("Running pass on nest {}", distance);

		osl_scop_p scop = nest->c_scop();

		int nest_count = osl_scop_count_loop_nests(scop);
		this->logger->debug("found {} nests", nest_count);
		if (nest_count <= 1) {
			auto nest = new OslScop(nextNestId++, scop);
			new_nests.push_back(nest);
			continue;
		}
		this->logger->debug("splitting nests...");

		osl_scop_p *new_scops = osl_scop_split_nests(scop);
		if (new_scops == NULL) {
			continue;
		}

		for (int i = 0; i < nest_count; i++) {
			auto nest = new OslScop(nextNestId++, new_scops[i]);
			nest->metadata.getOrCreate<metadata::OpenScopMetadata>().lock()->scops["original"] = osl_scop_clone(scop);

			new_nests.push_back(nest);
		}

		for (int i = 0; i < nest_count; i++) {
			osl_scop_free(new_scops[i]);
		}
		free(new_scops);
	}

	if (new_nests.size() > 0) {
		this->logger->info("Added {} new nests", new_nests.size());
	} else {
		this->logger->info("No new nests added");
		return;
	}
	prog.clearScopNests();
	for (auto &nest : new_nests) {
		prog.addScopNest(nest);
	}
}
