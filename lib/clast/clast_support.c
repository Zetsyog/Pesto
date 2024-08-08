#include <cloog/isl/cloog.h>
#include <isl/set.h>
#include <isl/val.h>

#include <assert.h>

#include "clast_support.h"

struct clast_stmt *clast_get_stmt_before(struct clast_stmt *root,
										 struct clast_stmt *stmt) {
	for (; root; root = root->next) {
		struct clast_stmt *syntax_next = clast_get_next(root);
		if (root->next == stmt || syntax_next == stmt) {
			return root;
		}
		if (clast_stmt_is_branch(stmt)) {
			struct clast_stmt *before =
				clast_get_stmt_before(syntax_next, stmt);
			if (before != NULL) {
				return before;
			}
		}
	}
	return NULL;
}

int clast_stmt_is_branch(struct clast_stmt *stmt) {
	return CLAST_STMT_IS_A(stmt, stmt_for) ||
		   CLAST_STMT_IS_A(stmt, stmt_guard) ||
		   CLAST_STMT_IS_A(stmt, stmt_block);
}

struct clast_stmt *clast_branch_deeper_next(struct clast_stmt *stmt) {
	if (CLAST_STMT_IS_A(stmt, stmt_for)) {
		return ((struct clast_for *)stmt)->body;
	} else if (CLAST_STMT_IS_A(stmt, stmt_guard)) {
		return ((struct clast_guard *)stmt)->then;
	} else if (CLAST_STMT_IS_A(stmt, stmt_block)) {
		return ((struct clast_block *)stmt)->body;
	} else {
		return NULL;
	}
}

struct clast_stmt **clast_get_next_ptr(struct clast_stmt *stmt) {
	if (CLAST_STMT_IS_A(stmt, stmt_for)) {
		return &((struct clast_for *)stmt)->body;
	} else if (CLAST_STMT_IS_A(stmt, stmt_guard)) {
		return &((struct clast_guard *)stmt)->then;
	} else if (CLAST_STMT_IS_A(stmt, stmt_block)) {
		return &((struct clast_block *)stmt)->body;
	} else {
		return &stmt->next;
	}
}

void clast_set_next(struct clast_stmt *stmt, struct clast_stmt *next) {
	*clast_get_next_ptr(stmt) = next;
}

struct clast_stmt *clast_insert_indepth(struct clast_stmt *before,
										struct clast_stmt *stmt) {
	stmt->next = clast_get_next(before);
	clast_set_next(before, stmt);
	return stmt;
}

struct clast_stmt *clast_get_next(struct clast_stmt *stmt) {
	return *clast_get_next_ptr(stmt);
}

struct clast_user_stmt *pesto_clast_create_stmt(CloogOptions *cloogOptions,
												CloogDomain *domain,
												unsigned int id) {
	CloogStatement *atiling_stmt =
		cloog_statement_alloc(cloogOptions->state, id);

	struct clast_user_stmt *user_stmt =
		new_clast_user_stmt(domain, atiling_stmt, NULL);
	cloog_statement_free(atiling_stmt);
	return user_stmt;
}

struct clast_stmt *clast_insert_after(struct clast_stmt *before,
									  struct clast_stmt *stmt) {
	assert(before != NULL);
	if (stmt != NULL) {
		stmt->next = before->next;
	}
	before->next = stmt;
	return stmt;
}

struct clast_stmt *clast_insert_before(struct clast_stmt *root,
									   struct clast_stmt *before,
									   struct clast_stmt *stmt) {
	assert(root != NULL);
	assert(before != NULL);

	while (root != NULL) {
		if (clast_stmt_is_branch(root)) {
			struct clast_stmt *deeper_stmt = clast_branch_deeper_next(root);
			if (deeper_stmt == before) {
				stmt->next = before;
				if (CLAST_STMT_IS_A(root, stmt_block)) {
					((struct clast_block *)root)->body = stmt;
				} else if (CLAST_STMT_IS_A(root, stmt_for)) {
					((struct clast_for *)root)->body = stmt;
				} else if (CLAST_STMT_IS_A(root, stmt_guard)) {
					((struct clast_guard *)root)->then = stmt;
				}
				return stmt;
			}
			struct clast_stmt *inserted_stmt =
				clast_insert_before(deeper_stmt, before, stmt);
			if (inserted_stmt != NULL) {
				return inserted_stmt;
			}
		}
		if (root->next == before) {
			stmt->next = before;
			root->next = stmt;
			return stmt;
		}

		root = root->next;
	}

	return NULL;
}

int clast_for_is_outter_tile_loop(struct clast_for *for_stmt) {
	assert(for_stmt->domain != NULL);

	isl_set *set = isl_set_from_cloog_domain(for_stmt->domain);
	isl_ctx *ctx = isl_set_get_ctx(set);

	isl_set *domain = isl_set_copy(set);

	// idx is the index of the iterator dimension in the domain
	int idx = isl_set_find_dim_by_name(domain, isl_dim_set, for_stmt->iterator);
	if (idx < 0) {
		isl_set_free(domain);
		return 0;
	}

	// only keep constraints involving the iterator
	domain = isl_set_drop_constraints_not_involving_dims(domain, isl_dim_set,
														 idx, 1);

	isl_basic_set_list *bset_list = isl_set_get_basic_set_list(domain);
	isl_size n_bset = isl_basic_set_list_n_basic_set(bset_list);

	int is_outter_tile = 0;

	for (int i = 0; i < n_bset; i += 1) {
		isl_basic_set *bset = isl_basic_set_list_get_at(bset_list, i);
		isl_constraint_list *cst_list = isl_basic_set_get_constraint_list(bset);
		isl_size n_cst = isl_constraint_list_n_constraint(cst_list);

		for (int j = 0; j < n_cst; j += 1) {
			isl_constraint *cst = isl_constraint_list_get_at(cst_list, j);
			isl_val *val =
				isl_constraint_get_coefficient_val(cst, isl_dim_set, idx);
			// TODO: remove hard-coded value
			if (isl_val_eq_si(val, 32) || isl_val_eq_si(val, -32)) {
				is_outter_tile = 1;
			}
			isl_val_free(val);
			isl_constraint_free(cst);
			if (is_outter_tile) {
				break;
			}
		}
		isl_constraint_list_free(cst_list);
		isl_basic_set_free(bset);
		if (is_outter_tile) {
			break;
		}
	}

	isl_basic_set_list_free(bset_list);
	isl_set_free(domain);

	return is_outter_tile;
}

int clast_for_is_inner_tile_loop(struct clast_for *for_stmt) {
	return clast_for_get_inner_tile_outter_dim(for_stmt) != -1;
}

int clast_for_get_inner_tile_outter_dim(struct clast_for *for_stmt) {
	assert(for_stmt->domain != NULL);

	isl_set *set = isl_set_from_cloog_domain(for_stmt->domain);
	isl_ctx *ctx = isl_set_get_ctx(set);

	isl_set *domain = isl_set_copy(set);

	int n_dim = isl_set_dim(domain, isl_dim_set);
	int idx = n_dim - 1;

	// only keep constraints involving the iterator
	domain = isl_set_drop_constraints_not_involving_dims(domain, isl_dim_set,
														 idx, 1);

	isl_basic_set_list *bset_list = isl_set_get_basic_set_list(domain);
	isl_size n_bset = isl_basic_set_list_n_basic_set(bset_list);

	int corresponding_outter_dim = -1;

	for (int i = 0; i < n_bset; i += 1) {
		isl_basic_set *bset = isl_basic_set_list_get_at(bset_list, i);
		isl_constraint_list *cst_list = isl_basic_set_get_constraint_list(bset);
		isl_size n_cst = isl_constraint_list_n_constraint(cst_list);

		for (int j = 0; j < n_cst; j += 1) {
			isl_constraint *cst = isl_constraint_list_get_at(cst_list, j);
			// if there is a constraint of the form
			// -32 * t[k] + t[idx] >= 0
			// or - t[idx] + 32 * t[k] + 31 >= 0
			// then this loop is tiled by the k-th dimension

			// first we iterate over all the dimensions
			for (int k = 0; k < n_dim; k += 1) {
				if (k == idx) {
					continue;
				}
				isl_val *kval =
					isl_constraint_get_coefficient_val(cst, isl_dim_set, k);
				isl_val *idxval =
					isl_constraint_get_coefficient_val(cst, isl_dim_set, idx);

				if (isl_val_eq_si(idxval, 1) && isl_val_eq_si(kval, -32)) {
					corresponding_outter_dim = k;
				}

				isl_val_free(kval);
				isl_val_free(idxval);

				if (corresponding_outter_dim != -1) {
					break;
				}
			}
			isl_constraint_free(cst);
		}
		isl_constraint_list_free(cst_list);
		isl_basic_set_free(bset);
		if (corresponding_outter_dim != -1) {
			break;
		}
	}

	isl_basic_set_list_free(bset_list);
	isl_set_free(domain);

	return corresponding_outter_dim;
}

struct clast_stmt **clast_get_user_stmt_list(struct clast_stmt *root,
											 int *size) {
	struct clast_stmt **stmts = NULL;
	*size = 0;

	// DFS algorithm to traverse the syntax tree
	for (; root != NULL; root = root->next) {
		if (CLAST_STMT_IS_A(root, stmt_user)) {
			stmts = realloc(stmts, (*size + 1) * sizeof(struct clast_stmt *));
			stmts[*size] = root;
			*size += 1;
		} else if (clast_stmt_is_branch(root)) {
			int branch_size = 0;
			struct clast_stmt **branch_stmts = clast_get_user_stmt_list(
				clast_branch_deeper_next(root), &branch_size);

			stmts = realloc(stmts, (*size + branch_size) *
									   sizeof(struct clast_stmt *));

			for (int i = 0; i < branch_size; i += 1) {
				stmts[*size + i] = branch_stmts[i];
			}
			*size += branch_size;
			free(branch_stmts);
		}
	}

	return stmts;
}

static struct clast_for *_clast_get_parent_for(struct clast_stmt *root,
											   struct clast_stmt *target,
											   struct clast_for *parent) {
	for (; root != NULL; root = root->next) {
		if (root == target) {
			return parent;
		}
		if (CLAST_STMT_IS_A(root, stmt_for)) {
			struct clast_for *for_stmt = (struct clast_for *)root;
			struct clast_for *result =
				_clast_get_parent_for(for_stmt->body, target, for_stmt);
			if (result != NULL) {
				return result;
			}
		} else if (clast_stmt_is_branch(root)) {
			struct clast_for *result = _clast_get_parent_for(
				clast_branch_deeper_next(root), target, parent);
			if (result != NULL) {
				return result;
			}
		}
	}
	return NULL;
}

struct clast_for *clast_get_parent_clast_for(struct clast_stmt *root,
											 struct clast_stmt *stmt) {
	return _clast_get_parent_for(root, stmt, NULL);
}

struct clast_for *clast_get_outter_tiling_for(struct clast_stmt *root,
											  struct clast_for *inner_for) {
	int outter_tile_idx = clast_for_get_inner_tile_outter_dim(inner_for);
	if (outter_tile_idx == -1) {
		return NULL;
	}
	struct clast_for *parent =
		clast_get_parent_clast_for(root, (struct clast_stmt *)inner_for);
	while (parent != NULL) {
		isl_set *set = isl_set_from_cloog_domain(parent->domain);
		isl_size ndim = isl_set_dim(set, isl_dim_set);
		if (ndim - 1 == outter_tile_idx) {
			return parent;
		}
		parent = clast_get_parent_clast_for(root, (struct clast_stmt *)parent);
	}
	return NULL;
}

int clast_for_depth(struct clast_stmt *root, struct clast_for *for_stmt) {
	int depth = 0;
	while (for_stmt != NULL) {
		depth += 1;
		for_stmt =
			clast_get_parent_clast_for(root, (struct clast_stmt *)for_stmt);
	}
	return depth;
}

struct clast_expr *
clast_for_compute_original_bound(CloogNames *names, struct clast_for *inner_for,
								 struct clast_for *outter_for, int bound) {
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

	isl_basic_set_list *bset_list = isl_set_get_basic_set_list(set);
	isl_basic_set *bset = isl_basic_set_list_get_at(bset_list, 0);

	isl_constraint_list *cst_list = isl_basic_set_get_constraint_list(bset);
	isl_size n_cst = isl_constraint_list_n_constraint(cst_list);

	isl_bool (*bound_fn)(isl_constraint *, enum isl_dim_type, unsigned);
	if (bound == 0) {
		bound_fn = isl_constraint_is_lower_bound;
	} else {
		bound_fn = isl_constraint_is_upper_bound;
	}

	struct clast_expr *result_expr = NULL;

	for (int i = 0; i < n_cst; i += 1) {
		isl_constraint *cst = isl_constraint_list_get_at(cst_list, i);

		if (bound_fn(cst, isl_dim_set, ndim - 1)) {
			struct clast_expr *expr = clast_bound_from_constraint(
				cloog_constraint_from_isl_constraint(cst), ndim, names);

			if (result_expr == NULL) {
				result_expr = expr;
			} else {
				struct clast_reduction *red = new_clast_reduction(
					bound == 0 ? clast_red_max : clast_red_min, 2);
				red->elts[0] = result_expr;
				red->elts[1] = expr;
				result_expr = (struct clast_expr *)red;
			}
		}
		isl_constraint_free(cst);
	}

	isl_constraint_list_free(cst_list);
	isl_basic_set_list_free(bset_list);
	isl_basic_set_free(bset);

	isl_set_free(set);

	return result_expr;
}
