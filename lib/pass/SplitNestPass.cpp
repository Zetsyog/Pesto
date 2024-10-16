#include <iostream>
#include <pesto/Pesto.hpp>

#include <osl_support.h>

using namespace pesto;

SplitNestedLoopsPass::SplitNestedLoopsPass() : IPestoPass("pass-split-nest") {}

SplitNestedLoopsPass::~SplitNestedLoopsPass() {}

void SplitNestedLoopsPass::run(PolyProgram &prog) {
	std::vector<ScopNest *> original_nests = prog.getScopNests();

	std::vector<ScopNest *> new_nests;
	int nextNestId = 0;

	// TODO: better algorithm ?
	for (auto it = original_nests.begin(); it != original_nests.end(); ++it) {
		auto &nest = *it;
		auto distance = std::distance(original_nests.begin(), it);

		log(std::cerr) << "Running pass on nest " << distance << std::endl;

		osl_scop_p scop = nest->getScop();

		int nest_count = osl_scop_count_loop_nests(scop);
		log(std::cerr) << "found " << nest_count << " nests" << std::endl;
		if (nest_count <= 1) {
			auto nest = new ScopNest(nextNestId++, scop);
			new_nests.push_back(nest);
			continue;
		}
		log(std::cerr) << "splitting nests..." << std::endl;

		osl_scop_p *new_scops = osl_scop_split_nests(scop);
		if (new_scops == NULL) {
			continue;
		}

		for (int i = 0; i < nest_count; i++) {
			auto nest = new ScopNest(nextNestId++, new_scops[i]);
			nest->metadata.getOrCreate<OpenScopMetadata>()
				.lock()
				->scops["original"] = osl_scop_clone(scop);

			new_nests.push_back(nest);
		}

		for (int i = 0; i < nest_count; i++) {
			osl_scop_free(new_scops[i]);
		}
		free(new_scops);
	}

	if (new_nests.size() > 0) {
		log(std::cerr) << "Added " << new_nests.size() << " new nests"
					   << std::endl;
	} else {
		log(std::cerr) << "No new nests added" << std::endl;
		return;
	}
	prog.clearScopNests();
	for (auto &nest : new_nests) {
		prog.addScopNest(nest);
	}
}
