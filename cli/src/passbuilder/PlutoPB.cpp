/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/pass/PlutoPass.hpp>
#include <pesto/passbuilder/PlutoPB.hpp>

using namespace pesto::builder;

#define PESTO_BIND_PLUTO_INTOPT(json_key, json_val, pluto_opt, name)                                                   \
	if (json_key == #name) {                                                                                           \
		if (!json_val.is_number_integer()) {                                                                           \
			logger->error("{} must be an integer.", json_key.c_str());                                                 \
			throw std::invalid_argument(std::string(json_key) + " must be an integer");                                \
		}                                                                                                              \
		pluto_opt.name = json_val;                                                                                     \
		continue;                                                                                                      \
	}
#define PESTO_BIND_PLUTO_BOOLOPT(json_key, json_val, pluto_opt, name)                                                  \
	if (json_key == #name) {                                                                                           \
		if (!json_val.is_boolean()) {                                                                                  \
			logger->error("{} must be a boolean.", json_key.c_str());                                                  \
			throw std::invalid_argument(std::string(json_key) + " must be a boolean");                                 \
		}                                                                                                              \
		pluto_opt.name = json_val;                                                                                     \
		continue;                                                                                                      \
	}

PlutoPB::PlutoPB() { logger = log::get_logger("pb/pluto"); }

PlutoPB::~PlutoPB() {}

std::unique_ptr<pesto::pass::IPass> PlutoPB::build(PassManagerBuilder &builder, const json &j) {
	(void)builder;

	// TODO Parse the JSON object and build the pass
	auto pass = std::make_unique<pesto::pass::PlutoPass>();
	if (!j.contains("options")) {
		return pass;
	}

	auto options = j["options"];
	if (!options.is_object()) {
		logger->error("\"options\" must be a JSON object");
		throw std::invalid_argument("\"options\" must be a JSON object");
	}

	// special iterator member functions for objects
	for (auto &[key, val] : options.items()) {
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), tile);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), intratileopt);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), diamondtile);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), per_cc_obj);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), dynschedule);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), dynschedule_graph_old);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), dyn_trans_deps_tasks);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), fulldiamondtile);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), parallel);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), innerpar);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), unrolljam);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), prevector);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), rar);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), delayed_cut);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), hybridcut);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), codegen_context);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), scancount);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), forceparallel);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), multipar);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), second_level_tile);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), find_tile_sizes);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), identity);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), bee);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), isldep);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), candldep);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), isldepaccesswise);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), isldepcoalesce);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), lastwriter);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), nodepbound);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), scalpriv);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), pipsolve);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), islsolve);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), glpk);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), gurobi);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), lp);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), dfp);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), ilp);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), lpcolour);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), scc_cluster);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), iss);
		PESTO_BIND_PLUTO_BOOLOPT(key, val, pass->getOptions(), flic);
		PESTO_BIND_PLUTO_INTOPT(key, val, pass->getOptions(), ft);
		PESTO_BIND_PLUTO_INTOPT(key, val, pass->getOptions(), lt);
		PESTO_BIND_PLUTO_INTOPT(key, val, pass->getOptions(), ufactor);
		PESTO_BIND_PLUTO_INTOPT(key, val, pass->getOptions(), coeff_bound);
		PESTO_BIND_PLUTO_INTOPT(key, val, pass->getOptions(), cache_size);
		PESTO_BIND_PLUTO_INTOPT(key, val, pass->getOptions(), data_element_size);

		if (key == "fuse") {
			if (!val.is_string()) {
				logger->error("'fuse' must be a string.");
				throw std::invalid_argument("'fuse' must be a string.");
			}
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
				logger->warn("Unknown value for 'fuse' option: {} ", fuse.c_str());
			}
			continue;
		}
		if (key == "tile-sizes") {
			if (!val.is_array() ||
				!std::all_of(val.begin(), val.end(), [](const json &v) { return v.is_number_integer(); })) {
				logger->error("'tile-sizes' must be an array of integers.");
				throw std::invalid_argument("'tile-sizes' must be an array of integers.");
			}
			std::vector<int> tile_sizes;
			val.get_to(tile_sizes);
			pass->getOptions().tile_sizes_len = tile_sizes.size();
			pass->getOptions().tile_sizes = (int *)malloc(tile_sizes.size() * sizeof(int));
			for (int i = 0; i < pass->getOptions().tile_sizes_len; i++) {
				pass->getOptions().tile_sizes[i] = tile_sizes[i];
			}
			continue;
		}
		// warnings for unhandled options
		if (key == "out_file") {
			logger->warn("Option 'out_file' is not handled by PlutoPassBuilder");
			continue;
		}

		if (key == "cloogf" || key == "cloogl" || key == "cloogsh" || key == "cloogbacktrack") {
			logger->warn("Cloog options are not handled by PlutoPassBuilder as Pluto does not directly generate code");
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
			logger->warn("Option 'readscop' is not handled by PlutoPassBuilder");
			continue;
		}
		if (key == "pet") {
			logger->warn("Option 'pet' is not handled by PlutoPassBuilder");
			continue;
		}
		logger->warn("Unknown pluto option: {}", key.c_str());
	}

	return pass;
}