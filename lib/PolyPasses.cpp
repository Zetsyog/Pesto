#include <attila/PolyhedralPass.hpp>
#include <iostream>

#include <osl_support.h>

SplitNestedLoopsPass::SplitNestedLoopsPass() {}

SplitNestedLoopsPass::~SplitNestedLoopsPass() {}

void SplitNestedLoopsPass::run(PolyProgram &prog) {
	std::vector<ScopNest> original_nests = prog.getScopNests();
	prog.clearScopNests();

	std::vector<ScopNest> new_nests;

	// TODO: better algorithm ?
	for (auto it = original_nests.begin(); it != original_nests.end(); ++it) {
		auto &nest = *it;
		auto distance = std::distance(original_nests.begin(), it);

		std::cerr << "[split-nested-loops] Running pass on nest " << distance
				  << std::endl;

		osl_scop_p scop = nest.getScop();

		int nest_count = osl_scop_count_loop_nests(scop);
		std::cerr << "[split-nested-loops] found " << nest_count << " nests"
				  << std::endl;
		if (nest_count <= 1) {
			continue;
		}
		std::cerr << "[split-nested-loops] splitting nests..." << std::endl;

		osl_scop_p *new_scops = osl_scop_split_nests(scop);
		if (new_scops == NULL) {
			continue;
		}

		for (int i = 0; i < nest_count; i++) {
			ScopNest new_nest(new_scops[i]);
			prog.addScopNest(new_nest);
		}

		osl_scop_free(scop);
	}
}

std::string SplitNestedLoopsPass::getUUID() { return "split-nested-loops"; }