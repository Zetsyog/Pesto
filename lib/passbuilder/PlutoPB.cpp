#include <pesto/Pesto.hpp>

using namespace pesto::builder;

#define PESTO_BIND_PLUTO_OPT(json, pluto_opt, name)                            \
	if (json.contains(#name)) {                                                \
		json.at(#name).get_to(pluto_opt.name);                                 \
	}

PlutoPB::PlutoPB() {}

PlutoPB::~PlutoPB() {}

std::unique_ptr<pesto::pass::IPass> PlutoPB::build(PassManagerBuilder &builder,
												   const json &j) {
	(void)builder;

	// TODO Parse the JSON object and build the pass
	auto pass = std::make_unique<pesto::pass::PlutoPass>();
	if (!j.contains("options")) {
		return pass;
	}

	auto options = j["options"];

	auto pluto_opt = options;

	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), tile);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), intratileopt);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), diamondtile);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), per_cc_obj);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), pet);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), dynschedule);
	// TODO: should this be parsed?
	// PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(),
	// dynschedule_graph_old);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), dyn_trans_deps_tasks);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), fulldiamondtile);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), parallel);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), innerpar);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), unrolljam);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), ufactor);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), prevector);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), rar);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), fuse);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), delayed_cut);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), hybridcut);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), scancount);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), codegen_context);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), forceparallel);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), multipar);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), second_level_tile);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), find_tile_sizes);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), cache_size);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), data_element_size);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), ft);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), lt);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), debug);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), moredebug);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), quiet);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), identity);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), bee);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), isldep);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), candldep);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), isldepaccesswise);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), isldepcoalesce);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), lastwriter);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), nodepbound);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), coeff_bound);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), scalpriv);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), silent);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), readscop);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), pipsolve);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), islsolve);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), glpk);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), gurobi);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), lp);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), dfp);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), ilp);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), lpcolour);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), scc_cluster);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), iss);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), time);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), flic);

	// warnings for unhandled options
	if (pluto_opt.contains("out_file")) {
		std::cerr
			<< "Warning: option 'out_file' is not handled by PlutoPassBuilder"
			<< std::endl;
	}

	if (pluto_opt.contains("cloogf") || pluto_opt.contains("cloogl") ||
		pluto_opt.contains("cloogsh") || pluto_opt.contains("cloogbacktrack")) {
		std::cerr << "Warning: options cloog are not handled by "
					 "PlutoPassBuilder as Pluto does not directly generate code"
				  << std::endl;
	}

	return pass;
}