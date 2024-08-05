#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <cloog/isl/cloog.h>

#include <pesto/Pesto.hpp>

#include "clast/clast_support.h"

#include "isl_support.h"

using namespace pesto;

ClastParamTilingPass::ClastParamTilingPass() {}

ClastParamTilingPass::~ClastParamTilingPass() {}

void ClastParamTilingPass::run(PolyProgram &prog) {
	// ensure each nest has a clast metadata
	for (auto &nest : prog.getScopNests()) {
		if (!nest->metadata.has("clast")) {
			log(std::cerr) << "Nest does not have clast metadata. You must "
							  "run the 'clast-gen' pass before this one."
						   << std::endl;
			log(std::cerr) << "Aborting pass." << std::endl;
			return;
		}
	}

	// run the pass on each nest
	for (auto &nest : prog.getScopNests()) {
		runSingleNest(*nest);
	}
}

void replace_outter_tiled_for_bounds(struct clast_for *for_stmt) {
	free_clast_expr(for_stmt->LB);
	free_clast_expr(for_stmt->UB);

	cloog_int_t zero;
	cloog_int_init(zero);
	for_stmt->LB = (struct clast_expr *)new_clast_term(zero, NULL);
	std::string ub_var = "ub" + std::string(for_stmt->iterator);
	for_stmt->UB = (struct clast_expr *)new_clast_name(strdup(ub_var.c_str()));
}

void replace_inner_tiled_for_bounds(ClastMetadata &metadata,
									struct clast_stmt *root,
									struct clast_for *inner_for,
									struct clast_for *outter_for) {
	int depth = clast_for_depth(root, inner_for);

	isl_set *set = isl_set_copy(isl_set_from_cloog_domain(inner_for->domain));
	isl_size ndim = isl_set_dim(set, isl_dim_set);
	isl_size ndim_outter =
		isl_set_dim(isl_set_from_cloog_domain(outter_for->domain), isl_dim_set);

	// keep only relation of current level
	set = isl_set_drop_constraints_not_involving_dims(set, isl_dim_set,
													  ndim - 1, 1);

	// remove constraints from the outter loop
	set = isl_set_drop_constraints_involving_dims(set, isl_dim_set,
												  ndim_outter - 1, 1);

	free_clast_expr(inner_for->LB);
	free_clast_expr(inner_for->UB);
	inner_for->LB = NULL;
	inner_for->UB = NULL;

	isl_basic_set_list *bset_list = isl_set_get_basic_set_list(set);
	isl_basic_set *bset = isl_basic_set_list_get_at(bset_list, 0);

	isl_constraint_list *cst_list = isl_basic_set_get_constraint_list(bset);
	isl_size n_cst = isl_constraint_list_n_constraint(cst_list);

	for (int i = 0; i < n_cst; i += 1) {
		isl_constraint *cst = isl_constraint_list_get_at(cst_list, i);
		isl_constraint_dump(cst);

		if (isl_constraint_is_lower_bound(cst, isl_dim_set, ndim - 1)) {
			struct clast_expr *expr = clast_bound_from_constraint(
				cloog_constraint_from_isl_constraint(cst), ndim,
				metadata.cloogProgram->names);

			if (inner_for->LB == NULL) {
				inner_for->LB = expr;
			} else {
				struct clast_reduction *red =
					new_clast_reduction(clast_red_max, 2);
				red->elts[0] = expr;
				red->elts[1] = inner_for->LB;
				inner_for->LB = (struct clast_expr *)red;
			}
		}

		if (isl_constraint_is_upper_bound(cst, isl_dim_set, ndim - 1)) {
			struct clast_expr *expr = clast_bound_from_constraint(
				cloog_constraint_from_isl_constraint(cst), ndim,
				metadata.cloogProgram->names);

			if (inner_for->UB == NULL) {
				inner_for->UB = expr;
			} else {
				struct clast_reduction *red =
					new_clast_reduction(clast_red_min, 2);
				red->elts[0] = expr;
				red->elts[1] = inner_for->UB;
				inner_for->UB = (struct clast_expr *)red;
			}
		}

		isl_constraint_free(cst);
	}

	std::string lb_var = "lb" + std::string(inner_for->iterator);
	struct clast_name *param_lb = new_clast_name(strdup(lb_var.c_str()));

	std::string ub_var = "ub" + std::string(inner_for->iterator);
	struct clast_name *param_ub = new_clast_name(strdup(ub_var.c_str()));

	if (inner_for->LB == NULL) {
		inner_for->LB = (struct clast_expr *)param_lb;
	} else {
		struct clast_reduction *red = new_clast_reduction(clast_red_max, 2);
		red->elts[0] = (struct clast_expr *)param_lb;
		red->elts[1] = inner_for->LB;
		inner_for->LB = (struct clast_expr *)red;
	}

	if (inner_for->UB == NULL) {
		inner_for->UB = (struct clast_expr *)param_ub;
	} else {
		struct clast_reduction *red = new_clast_reduction(clast_red_min, 2);
		red->elts[0] = (struct clast_expr *)param_ub;
		red->elts[1] = inner_for->UB;
		inner_for->UB = (struct clast_expr *)red;
	}

	isl_constraint_list_free(cst_list);
	isl_basic_set_list_free(bset_list);
	isl_basic_set_free(bset);

	isl_set_free(set);
}

void insert_tiling_statements(ScopNest &nest, struct clast_stmt *root,
							  struct clast_for *inner_for,
							  struct clast_for *outter_for) {
	ClastMetadata metadata = nest.metadata.get<ClastMetadata>("clast");
	std::vector<std::string> macros;
	if (nest.metadata.has(PESTO_UUID_METADATA_MACROS)) {
		macros = nest.metadata.get<std::vector<std::string>>(
			PESTO_UUID_METADATA_MACROS);
	}
	struct clast_user_stmt *header_stmt = pesto_clast_create_stmt(
		metadata.cloogOptions, NULL, ++metadata.stmtsNb);
	clast_insert_before(root, (struct clast_stmt *)outter_for,
						(struct clast_stmt *)header_stmt);

	struct trahrhe_codegen_data gen_data = {};
	gen_data.stmt_id = metadata.stmtsNb;
	gen_data.depth = 0;
	gen_data.endline = strdup("\\\n");
	gen_data.iterator_name = strdup(inner_for->iterator);
	gen_data.npar = metadata.cloogProgram->names->nb_parameters;
	gen_data.params = (char **)malloc(gen_data.npar * sizeof(char *));
	for (int i = 0; i < gen_data.npar; i++) {
		gen_data.params[i] =
			strdup(metadata.cloogProgram->names->parameters[i]);
	}

	// char buffer[256] = {0};
	// FILE *buffer_fp = fmemopen(buffer, 256, "w");
	// trahrhe_codegen_stmt(buffer_fp, &gen_data);
	// fclose(buffer_fp);
	// std::string macro = "#define S" + std::to_string(gen_data.stmt_id) + "()
	// " + 					std::string(buffer); macros.push_back(macro);

	nest.metadata.set(PESTO_UUID_METADATA_MACROS, macros);
}

void ClastParamTilingPass::runSingleNest(ScopNest &nest) {
	ClastMetadata metadata = nest.metadata.get<ClastMetadata>("clast");
	struct clast_root *root = (struct clast_root *)metadata.root;
	if (root == NULL) {
		return;
	}
	log(std::cerr) << "Running pass on nest " << nest.getId() << std::endl;

	int stmt_count = 0;
	struct clast_stmt **stmt =
		clast_get_user_stmt_list((struct clast_stmt *)root, &stmt_count);

	std::vector<struct clast_stmt *> stmt_to_process;
	for (int i = 0; i < stmt_count; i++) {
		stmt_to_process.push_back(stmt[i]);
	}

	while (stmt_to_process.size() != 0) {
		// group statement that have same for parent (ie same domain)
		std::vector<struct clast_stmt *> stmt_group;
		stmt_group.push_back(stmt_to_process.back());
		stmt_to_process.pop_back();

		struct clast_for *parent_for = clast_get_parent_clast_for(
			(struct clast_stmt *)root,
			(struct clast_stmt *)stmt_group.begin()[0]);

		for (std::vector<struct clast_stmt *>::iterator it =
				 stmt_to_process.begin();
			 it != stmt_to_process.end();) {
			struct clast_for *tmp = clast_get_parent_clast_for(
				(struct clast_stmt *)root, (struct clast_stmt *)*it);
			if (parent_for == tmp) { // Remove even numbers
				stmt_group.push_back(*it);
				it = stmt_to_process.erase(
					it); // `erase` returns the next iterator
			} else {
				++it; // Increment the iterator only if no element was
					  // erased
			}
		}

		// dump the group
		std::cerr << "Group size: " << stmt_group.size() << std::endl;
		for (struct clast_stmt *stmt : stmt_group) {
			clast_pprint(stderr, stmt, 0, metadata.cloogOptions);
		}

		// apply tiling
		while (parent_for != NULL) {
			if (clast_for_is_inner_tile_loop(parent_for)) {
				std::cerr << parent_for->iterator << " is inner tile loop ";

				struct clast_for *outter_for = clast_get_outter_tiling_for(
					(struct clast_stmt *)root, parent_for);
				if (outter_for == NULL) {
					parent_for = clast_get_parent_clast_for(
						(struct clast_stmt *)root,
						(struct clast_stmt *)parent_for);
					continue;
				}
				std::cerr << " Outter loop: ";
				std::cerr << outter_for->iterator << std::endl;

				// replace the bounds of the outter loop
				replace_outter_tiled_for_bounds(outter_for);
				replace_inner_tiled_for_bounds(metadata,
											   (struct clast_stmt *)root,
											   parent_for, outter_for);
				insert_tiling_statements(nest, (struct clast_stmt *)root,
										 parent_for, outter_for);
			}
			parent_for = clast_get_parent_clast_for(
				(struct clast_stmt *)root, (struct clast_stmt *)parent_for);
		}
	}
}

std::string ClastParamTilingPass::getUUID() { return "clast-param-tiling"; }