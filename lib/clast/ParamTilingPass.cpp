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

enum IteratorType { IT_DEFAULT, IT_INNERTILE, IT_OUTERTILE };

/**
 * ClastIteratorInfo
 * A class to store information about an iterator in a clast tree
 * It will store dependencies between tiling and tiled iterators
 */
class ClastIteratorInfo {
  public:
	struct clast_for *for_stmt;
	int depth;
	std::string name;
	IteratorType type;
	std::vector<std::shared_ptr<ClastIteratorInfo>> depends_on;

	void dump(std::ostream &os) {
		os << "Iterator: " << name << std::endl;
		os << "\tDepth: " << depth << std::endl;
		os << "\tKind: ";
		switch (type) {
		case IT_DEFAULT:
			os << "Default";
			break;
		case IT_INNERTILE:
			os << "Inner Tile";
			break;
		case IT_OUTERTILE:
			os << "Outer Tile";
			break;
		}
		os << std::endl;
		if (depends_on.size() > 0) {
			os << "\tDepends on: ";
			for (auto &dep : depends_on) {
				os << dep->for_stmt->iterator << " ";
			}
			os << std::endl;
		}
		os << std::endl;
	}
};

class IteratorInfoList {
  public:
	std::vector<std::shared_ptr<ClastIteratorInfo>> iterators;

	ClastIteratorInfo &getInfoFromClastFor(struct clast_for *for_stmt) {
		for (auto &info : iterators) {
			if (info->for_stmt == for_stmt) {
				return *info;
			}
		}
	}

	void setClastForKind(struct clast_for *for_stmt, IteratorType type) {
		for (auto &info : iterators) {
			if (info->for_stmt == for_stmt) {
				info->type = type;
			}
		}
	}

	void addIteratorDependency(struct clast_for *for_stmt,
							   struct clast_for *depends_on) {
		std::shared_ptr<ClastIteratorInfo> dep = nullptr;
		for (auto &info : iterators) {
			if (info->for_stmt == depends_on) {
				dep = info;
				break;
			}
		}
		for (auto &info : iterators) {
			if (info->for_stmt == for_stmt) {
				for (auto &d : info->depends_on) {
					if (d == dep) {
						return;
					}
				}
				info->depends_on.push_back(dep);
				return;
			}
		}
	}

	std::shared_ptr<ClastIteratorInfo> getInfoFromName(std::string name) {
		for (auto &info : iterators) {
			if (info->name == name) {
				return info;
			}
		}
		return nullptr;
	}

	void add(int depth, struct clast_for *for_stmt, std::string name,
			 IteratorType type) {
		auto info = std::make_shared<ClastIteratorInfo>();
		info->depth = depth;
		info->for_stmt = for_stmt;
		info->name = name;
		info->type = type;
		iterators.push_back(std::move(info));
	}
};

void traverseClastNode(std::function<void(struct clast_stmt *, int)> cb,
					   struct clast_stmt *node, int depth = 0) {
	if (node == nullptr) {
		return;
	}
	cb(node, depth);
	if (CLAST_STMT_IS_A(node, stmt_for)) {
		struct clast_for *for_stmt = (struct clast_for *)node;
		struct clast_stmt *body = for_stmt->body;
		while (body != nullptr) {
			traverseClastNode(cb, body, depth + 1);
			body = body->next;
		}
	}
	if (CLAST_STMT_IS_A(node, stmt_guard)) {
		struct clast_guard *guard_stmt = (struct clast_guard *)node;
		struct clast_stmt *body = guard_stmt->then;
		while (body != nullptr) {
			traverseClastNode(cb, body, depth);
			body = body->next;
		}
	}
}

void traverseClastExpr(std::function<int(struct clast_expr *)> cb,
					   struct clast_expr *expr) {
	if (expr == nullptr) {
		return;
	}
	int quit = cb(expr);
	if (quit) {
		return;
	}
	if (expr->type == clast_expr_name) {
		return;
	}
	if (expr->type == clast_expr_term) {
		struct clast_term *term = (struct clast_term *)expr;
		if (term->var != nullptr) {
			traverseClastExpr(cb, term->var);
		}
	}
	if (expr->type == clast_expr_bin) {
		struct clast_binary *bin = (struct clast_binary *)expr;
		if (bin->LHS != nullptr) {
			traverseClastExpr(cb, bin->LHS);
		}
	}
	if (expr->type == clast_expr_red) {
		struct clast_reduction *red = (struct clast_reduction *)expr;
		for (int i = 0; i < red->n; i++) {
			traverseClastExpr(cb, red->elts[i]);
		}
	}
}

std::shared_ptr<IteratorInfoList>
retrieveIteratorInfo(ScopNest &nest, struct clast_stmt *node) {
	osl_scop_p scop = nest.getScop();
	auto infoList = std::make_shared<IteratorInfoList>();

	std::vector<std::string> names;

	traverseClastNode(
		[&](struct clast_stmt *node, int depth) {
			if (CLAST_STMT_IS_A(node, stmt_for)) {
				struct clast_for *for_stmt = (struct clast_for *)node;
				infoList->add(depth, for_stmt, std::string(for_stmt->iterator),
							  IT_DEFAULT);
				names.push_back(std::string(for_stmt->iterator));
				isl_set *set = isl_set_from_cloog_domain(for_stmt->domain);
				set = isl_set_set_param_names_from_osl_scop(set, scop);
				isl_size ndim = isl_set_dim(set, isl_dim_set);
				// iterate through names
				for (isl_size i = 0; i < ndim; i += 1) {
					set = isl_set_set_dim_name(set, isl_dim_set, i,
											   names.begin()[i].c_str());
				}
				for_stmt->domain = cloog_domain_from_isl_set(set);
			}
		},
		node);
	// find iterator types
	for (auto &info : infoList->iterators) {
		traverseClastNode(
			[&](struct clast_stmt *node, int depth) {
				if (CLAST_STMT_IS_A(node, stmt_for)) {
					struct clast_for *for_stmt = (struct clast_for *)node;
					if (clast_for_is_outter_tile_loop(for_stmt)) {
						infoList->setClastForKind(for_stmt, IT_OUTERTILE);
					}
					int corresponding_dim =
						clast_for_get_inner_tile_outter_dim(for_stmt);
					if (corresponding_dim != -1) {
						infoList->setClastForKind(for_stmt, IT_INNERTILE);
						isl_set *set =
							isl_set_from_cloog_domain(for_stmt->domain);
						std::string name(isl_set_get_dim_name(
							set, isl_dim_set, corresponding_dim));
						auto dep = infoList->getInfoFromName(name);
						if (dep != nullptr) {
							infoList->addIteratorDependency(for_stmt,
															dep->for_stmt);
						}
					}
				}
			},
			(struct clast_stmt *)info->for_stmt);
	}
	return infoList;
}

/**
 * Insert statements where tiling bounds will be computed
 * TODO: handle wavefront paralellism
 */
void insert_tiling_statements(ScopNest &nest,
							  std::shared_ptr<IteratorInfoList> itInfo) {
	ClastMetadata data = nest.metadata.get<ClastMetadata>("clast");
	osl_scop_p scop = nest.getScop();
	struct clast_stmt *root = data.root;
	assert(root != NULL);

	std::vector<std::string> macros;
	if (nest.metadata.has(PESTO_UUID_METADATA_MACROS)) {
		macros = nest.metadata.get<std::vector<std::string>>(
			PESTO_UUID_METADATA_MACROS);
	}

	int npar = osl_scop_get_nb_parameters(scop);
	std::string fun_prefix = "n" + std::to_string(nest.getId());

	// for each tiling iterator, we should insert a stmt before (header) and
	// inside (footer)
	for (auto &it : itInfo->iterators) {
		if (it->type == IT_OUTERTILE) {
			struct clast_for *for_loop = it->for_stmt;
			struct trahrhe_codegen_data gen_data[2];

			struct clast_user_stmt *header_stmt = pesto_clast_create_stmt(
				data.cloogOptions, for_loop->domain, ++data.stmtsNb);
			clast_insert_before(root, (struct clast_stmt *)for_loop,
								(struct clast_stmt *)header_stmt);
			gen_data[0].stmt_type = head;
			gen_data[0].tiling_type = algebraic;
			gen_data[0].stmt_id = data.stmtsNb;
			gen_data[0].iterator_name = strdup(for_loop->iterator);
			// TODO
			gen_data[0].depth = it->depth;
			gen_data[0].lb_expr = strdup("lb");
			gen_data[0].ub_expr = strdup("ub");
			gen_data[0].var_prefix = strdup("");
			gen_data[0].fun_prefix = (char *)fun_prefix.c_str();
			gen_data[0].npar = npar;
			gen_data[0].params =
				(char **)malloc(gen_data[0].npar * sizeof(char *));
			for (int i = 0; i < gen_data[0].npar; i += 1) {
				gen_data[0].params[i] =
					strdup(((osl_strings_p)scop->parameters->data)->string[i]);
			}

			gen_data[0].endline = strdup("\\\n");

			struct clast_user_stmt *footer_stmt = pesto_clast_create_stmt(
				data.cloogOptions, for_loop->domain, ++data.stmtsNb);
			clast_insert_indepth((struct clast_stmt *)for_loop,
								 (struct clast_stmt *)footer_stmt);
			gen_data[1].stmt_type = body;
			gen_data[1].tiling_type = algebraic;
			gen_data[1].stmt_id = data.stmtsNb;
			gen_data[1].iterator_name = strdup(for_loop->iterator);
			// TODO
			gen_data[1].var_prefix = strdup("");
			gen_data[1].fun_prefix = (char *)fun_prefix.c_str();
			gen_data[1].depth = it->depth;
			gen_data[1].lb_expr = strdup("lb");
			gen_data[1].ub_expr = strdup("ub");
			gen_data[1].npar = 0;
			gen_data[1].params = NULL;
			gen_data[1].endline = strdup("\\\n");

			for (int i = 0; i < 2; i += 1) {
				char buffer[256] = {0};
				FILE *buffer_fp = fmemopen(buffer, 256, "w");
				trahrhe_codegen_stmt(buffer_fp, &gen_data[i]);
				fclose(buffer_fp);
				std::string macro = "#define S" +
									std::to_string(gen_data[i].stmt_id) +
									"() " + std::string(buffer);
				macros.push_back(macro);
			}

			for (int i = 0; i < 1; i += 1) {
				free(gen_data[i].iterator_name);
				free(gen_data[i].lb_expr);
				free(gen_data[i].ub_expr);
				free(gen_data[i].endline);
				for (int j = 0; j < gen_data[i].npar; j += 1) {
					if (gen_data[i].params[j] != NULL)
						free(gen_data[i].params[j]);
				}
				if (gen_data[i].params != NULL)
					free(gen_data[i].params);
			}
		}
	}

	nest.metadata.set(PESTO_UUID_METADATA_MACROS, macros);
}

void replace_outter_tiling_loop_bounds(
	ScopNest &nest, std::shared_ptr<IteratorInfoList> itInfo) {
	for (auto &it : itInfo->iterators) {
		if (it->type == IT_OUTERTILE) {
			struct clast_for *for_loop = it->for_stmt;
			struct clast_expr *old_lb = for_loop->LB;
			struct clast_expr *old_ub = for_loop->UB;

			cloog_int_t zero;
			cloog_int_init(zero);
			struct clast_term *new_lb = new_clast_term(zero, NULL);
			std::string ub_var = "ubt" + std::to_string(it->depth);
			struct clast_name *new_ub = new_clast_name(strdup(ub_var.c_str()));

			for_loop->LB = (struct clast_expr *)new_lb;
			for_loop->UB = (struct clast_expr *)new_ub;

			// free old bounds
			free_clast_expr(old_lb);
			free_clast_expr(old_ub);
		}
	}
}
void replace_inner_tiling_loop_bounds(
	ScopNest &nest, ClastMetadata &clastData,
	std::shared_ptr<IteratorInfoList> itInfo) {
	if (!nest.metadata.has(PESTO_UUID_METADATA_OSL_MAINSCOP)) {
		std::cerr << "[clast-param-tiling] WARNING: No main scop found in nest "
				  << nest.getId() << std::endl;
		return;
	}
	osl_scop_p scop = nest.getScop();

	osl_scop_p mainScop =
		nest.metadata.get<osl_scop_p>(PESTO_UUID_METADATA_OSL_MAINSCOP);
	osl_scatnames_p scatnames =
		(osl_scatnames_p)osl_generic_lookup(scop->extension, OSL_URI_SCATNAMES);

	struct clast_root *root = (struct clast_root *)clastData.root;

	// osl_scop_dump(stderr, scop);

	isl_ctx *ctx = isl_ctx_alloc();

	isl_basic_set *domain = isl_basic_set_from_osl_scop(ctx, mainScop);

	isl_basic_set_dump(domain);

	isl_basic_set_free(domain);

	for (auto &it : itInfo->iterators) {
		if (it->type != IT_INNERTILE) {
			continue;
		}
		fprintf(stderr, "Iterator: %s\n", it->for_stmt->iterator);
		isl_set *set =
			isl_set_copy(isl_set_from_cloog_domain(it->for_stmt->domain));

		int current_dim = 0;
		isl_size ndim = isl_set_dim(set, isl_dim_set);
		// set scattering iterator names if scat dims
		if (scatnames != NULL) {
			for (int i = 0; i < ndim; i += 1) {
				std::string iterator_name = scatnames->names->string[i];
				if (iterator_name == it->name) {
					current_dim = i;
				}
				// set = isl_set_set_dim_name(set, isl_dim_set, i,
				// 						   iterator_name.c_str());
			}
		}
		free_clast_expr(it->for_stmt->LB);
		free_clast_expr(it->for_stmt->UB);
		it->for_stmt->LB = NULL;
		it->for_stmt->UB = NULL;

		// cleanup some relations
		set = isl_set_drop_constraints_not_involving_dims(set, isl_dim_set,
														  current_dim, 1);

		for (auto &it2 : itInfo->iterators) {
			if (it == it2) {
				continue;
			}
			if (it2->type != IT_OUTERTILE) {
				continue;
			}
			set = isl_set_drop_constraints_involving_dims(set, isl_dim_set,
														  it2->depth, 1);
		}

		// find original bounds without tiling
		isl_basic_set_list *bset_list = isl_set_get_basic_set_list(set);
		isl_size nbset = isl_basic_set_list_size(bset_list);
		for (isl_size i = 0; i < nbset; i += 1) {
			isl_basic_set *bset = isl_basic_set_list_get_at(bset_list, i);

			isl_constraint_list *cst_list =
				isl_basic_set_get_constraint_list(bset);
			isl_size nconstr = isl_basic_set_n_constraint(bset);
			int level = it->depth + 1;
			for (isl_size j = 0; j < nconstr; j += 1) {
				isl_constraint *cst = isl_constraint_list_get_at(cst_list, j);
				struct clast_expr *expr = clast_bound_from_constraint(
					cloog_constraint_from_isl_constraint(cst), level,
					root->names);
				if (isl_constraint_is_upper_bound(cst, isl_dim_set,
												  current_dim)) {
					if (it->for_stmt->UB == NULL) {
						it->for_stmt->UB = expr;
					} else {
						struct clast_reduction *red =
							new_clast_reduction(clast_red_min, 2);
						red->elts[0] = expr;
						red->elts[1] = it->for_stmt->UB;
						it->for_stmt->UB = (struct clast_expr *)red;
					}
				}
				if (isl_constraint_is_lower_bound(cst, isl_dim_set,
												  current_dim)) {
					if (it->for_stmt->LB == NULL) {
						it->for_stmt->LB = expr;
					} else {
						struct clast_reduction *red =
							new_clast_reduction(clast_red_max, 2);
						red->elts[0] = expr;
						red->elts[1] = it->for_stmt->LB;
						it->for_stmt->LB = (struct clast_expr *)red;
					}
				}

				isl_constraint_free(cst);
				clast_pprint_expr(clastData.cloogOptions, stderr, expr);
				fprintf(stderr, "\n");
			}
			isl_constraint_list_free(cst_list);
			isl_basic_set_free(bset);

			if (it->for_stmt->LB == NULL) {
				cloog_int_t zero;
				cloog_int_init(zero);
				cloog_int_set_si(zero, 0);
				struct clast_term *term = new_clast_term(zero, NULL);
				it->for_stmt->LB = (struct clast_expr *)term;
			}

			for (auto &dep : it->depends_on) {
				struct clast_reduction *red =
					new_clast_reduction(clast_red_max, 2);
				std::string lb_name = "lb" + std::to_string(dep->depth);
				struct clast_name *tile_lb =
					new_clast_name(strdup(lb_name.c_str()));

				red->elts[0] = it->for_stmt->LB;
				red->elts[1] = (struct clast_expr *)tile_lb;

				it->for_stmt->LB = (struct clast_expr *)red;
			}
		}

		isl_basic_set_list_free(bset_list);
		isl_set_free(set);
	}
	isl_ctx_free(ctx);
}

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

void ClastParamTilingPass::runSingleNest(ScopNest &nest) {
	ClastMetadata metadata = nest.metadata.get<ClastMetadata>("clast");
	struct clast_root *root = (struct clast_root *)metadata.root;
	if (root == NULL) {
		return;
	}
	log(std::cerr) << "Running pass on nest " << nest.getId() << std::endl;

	// retrieve data about iterators
	auto itInfo = retrieveIteratorInfo(nest, root->stmt.next);

	// dump iterator info
	// DEBUG
	// TODO: remove this
	for (auto &info : itInfo->iterators) {
		info->dump(std::cerr);
	}

	insert_tiling_statements(nest, itInfo);
	replace_outter_tiling_loop_bounds(nest, itInfo);
	replace_inner_tiling_loop_bounds(nest, metadata, itInfo);
}

std::string ClastParamTilingPass::getUUID() { return "clast-param-tiling"; }