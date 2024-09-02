#include <iostream>
#include <pesto/Pesto.hpp>

#include <osl_support.h>

using namespace pesto;

SplitNestedLoopsPass::SplitNestedLoopsPass()
	: IPolyhedralPass("pass-split-nest") {}

SplitNestedLoopsPass::~SplitNestedLoopsPass() {}

void SplitNestedLoopsPass::run(PolyProgram &prog) {
	std::vector<ScopNest *> original_nests = prog.getScopNests();

	std::vector<ScopNest *> new_nests;
	int nextNestId = 0;

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
			auto nest = new ScopNest(nextNestId++, scop);
			new_nests.push_back(nest);
			continue;
		}
		std::cerr << "[split-nested-loops] splitting nests..." << std::endl;

		osl_scop_p *new_scops = osl_scop_split_nests(scop);
		if (new_scops == NULL) {
			continue;
		}

		for (int i = 0; i < nest_count; i++) {
			auto nest = new ScopNest(nextNestId++, new_scops[i]);
			new_nests.push_back(nest);
		}

		for (int i = 0; i < nest_count; i++) {
			osl_scop_free(new_scops[i]);
		}
		free(new_scops);
	}

	if (new_nests.size() > 0) {
		std::cerr << "[split-nested-loops] Added " << new_nests.size()
				  << " new nests" << std::endl;
	} else {
		std::cerr << "[split-nested-loops] No new nests added" << std::endl;
		return;
	}
	prog.clearScopNests();
	for (auto &nest : new_nests) {
		prog.addScopNest(nest);
	}
}

// ParamContextPass
ParamContextPass::ParamContextPass(int contextValue)
	: contextValue(contextValue), IPolySingleNestPass("pass-param-context") {}

ParamContextPass::~ParamContextPass() {}

void ParamContextPass::runSingleNest(ScopNest &nest) {
	osl_scop_p scop = nest.getScop();
	int nparam = osl_scop_get_nb_parameters(scop);
	for (int i = 0; i < nparam; i += 1) {
		osl_relation_insert_blank_row(scop->context, scop->context->nb_rows);
		osl_int_set_long(scop->context->precision,
						 &scop->context->m[scop->context->nb_rows - 1][0], 1);
		osl_int_set_long(scop->context->precision,
						 &scop->context->m[scop->context->nb_rows - 1][1 + i],
						 1);
		osl_int_set_long(
			scop->context->precision,
			&scop->context->m[scop->context->nb_rows - 1][1 + nparam],
			-this->contextValue);
	}
}

// DumpOslPass

DumpOslPass::DumpOslPass(std::string prefix)
	: path_prefix(prefix), IPolySingleNestPass("pass-dump-osl") {}

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
