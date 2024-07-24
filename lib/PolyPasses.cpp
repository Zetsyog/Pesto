#include <iostream>
#include <pesto/PolyhedralPass.hpp>

#include <osl_support.h>

SplitNestedLoopsPass::SplitNestedLoopsPass() {}

SplitNestedLoopsPass::~SplitNestedLoopsPass() {}

void SplitNestedLoopsPass::run(PolyProgram &prog) {
	std::vector<ScopNest *> original_nests = prog.getScopNests();

	std::vector<ScopNest *> new_nests;

	// TODO: better algorithm ?
	for (auto it = original_nests.begin(); it != original_nests.end(); ++it) {
		auto &nest = *it;
		auto distance = std::distance(original_nests.begin(), it);

		std::cerr << "[split-nested-loops] Running pass on nest " << distance
				  << std::endl;

		osl_scop_p scop = nest->getScop();

		int nest_count = osl_scop_count_loop_nests(scop);
		std::cerr << "[split-nested-loops] found " << nest_count << " nests"
				  << std::endl;
		if (nest_count <= 1) {
			new_nests.push_back(new ScopNest(scop));
			continue;
		}
		std::cerr << "[split-nested-loops] splitting nests..." << std::endl;

		osl_scop_p *new_scops = osl_scop_split_nests(scop);
		if (new_scops == NULL) {
			continue;
		}

		for (int i = 0; i < nest_count; i++) {
			new_nests.push_back(new ScopNest(new_scops[i]));
		}

		for (int i = 0; i < nest_count; i++) {
			osl_scop_free(new_scops[i]);
		}
	}

	if(new_nests.size() > 0) {
		std::cerr << "[split-nested-loops] Added " << new_nests.size() << " new nests" << std::endl;
	} else {
		std::cerr << "[split-nested-loops] No new nests added" << std::endl;
	}
	prog.clearScopNests();
	for(auto &nest : new_nests) {
		prog.addScopNest(nest);
	}
}

std::string SplitNestedLoopsPass::getUUID() { return "split-nested-loops"; }