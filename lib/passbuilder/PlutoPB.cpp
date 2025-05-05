#include <pesto/Pesto.hpp>

using namespace pesto::builder;

#define PESTO_BIND_PLUTO_OPT(json_key, json_val, pluto_opt, name)              \
	if (json_key == #name) {                                                   \
		pluto_opt.name = json_val;                                             \
		continue;                                                              \
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
	if (!options.is_object()) {
		std::cerr << "Warning: options should be a JSON object" << std::endl;
		return pass;
	}

	// special iterator member functions for objects
	for (json::iterator it = options.begin(); it != options.end(); ++it) {
		std::string key = it.key();
		auto val = it.value();
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), tile);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), intratileopt);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), diamondtile);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), per_cc_obj);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), dynschedule);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(),
							 dynschedule_graph_old);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(),
							 dyn_trans_deps_tasks);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), fulldiamondtile);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), parallel);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), innerpar);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), unrolljam);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), ufactor);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), prevector);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), rar);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), delayed_cut);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), hybridcut);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), scancount);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), codegen_context);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), forceparallel);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), multipar);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), second_level_tile);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), find_tile_sizes);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), cache_size);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), data_element_size);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), ft);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), lt);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), identity);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), bee);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), isldep);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), candldep);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), isldepaccesswise);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), isldepcoalesce);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), lastwriter);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), nodepbound);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), coeff_bound);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), scalpriv);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), pipsolve);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), islsolve);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), glpk);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), gurobi);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), lp);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), dfp);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), ilp);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), lpcolour);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), scc_cluster);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), iss);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), time);
		PESTO_BIND_PLUTO_OPT(key, val, pass->getOptions(), flic);
		if (key == "fuse") {
			if (!val.is_string()) {
				std::cerr
					<< "Warning: option 'fuse' should be a string. Skipping."
					<< std::endl;
				continue;
			} else {
				std::string fuse = options["fuse"];
				if (fuse == "none") {
					pass->getOptions().fuse = kNoFuse;
				} else if (fuse == "maximal") {
					pass->getOptions().fuse = kMaximalFuse;
				} else if (fuse == "smart") {
					pass->getOptions().fuse = kSmartFuse;
				} else if (fuse == "typed") {
					pass->getOptions().fuse = kTypedFuse;
				} else if (fuse == "hybrid") {
					pass->getOptions().fuse = kHybridFuse;
				} else {
					std::cerr
						<< "Warning: unknown value for 'fuse' option: " << fuse
						<< std::endl;
				}
			}
			continue;
		}
		if (key == "tile-sizes") {
			std::vector<int> tile_sizes;
			val.get_to(tile_sizes);
			pass->getOptions().tile_sizes_len = tile_sizes.size();
			pass->getOptions().tile_sizes =
				(int *)malloc(tile_sizes.size() * sizeof(int));
			for (int i = 0; i < pass->getOptions().tile_sizes_len; i++) {
				pass->getOptions().tile_sizes[i] = tile_sizes[i];
			}
			continue;
		}
		// warnings for unhandled options
		if (key == "out_file") {
			std::cerr << "Warning: option 'out_file' is not handled by "
						 "PlutoPassBuilder"
					  << std::endl;
			continue;
		}

		if (key == "cloogf" || key == "cloogl" || key == "cloogsh" ||
			key == "cloogbacktrack") {
			std::cerr
				<< "Warning: cloog options are not handled by "
				   "PlutoPassBuilder as Pluto does not directly generate code"
				<< std::endl;
			continue;
		}
		if (key == "silent") {
			pass->getOptions().silent = val;
			pass->getOptions().quiet = val;
			continue;
		}
		if (key == "debug") {
			pass->getOptions().debug = val;
			pass->getOptions().moredebug = val;
			continue;
		}
		if (key == "readscop") {
			std::cerr << "Warning: option 'readscop' is not handled by "
						 "PlutoPassBuilder"
					  << std::endl;
			continue;
		}
		if (key == "pet") {
			std::cerr << "Warning: option 'pet' is not handled by "
						 "PlutoPassBuilder"
					  << std::endl;
			continue;
		}
		std::cerr << "Warning: unknown pluto option '" << key << "'"
				  << std::endl;
	}

	return pass;
}