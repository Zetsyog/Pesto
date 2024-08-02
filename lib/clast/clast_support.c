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

	// idx is the index of the iterator dimension in the domain
	int idx = isl_set_find_dim_by_name(domain, isl_dim_set, for_stmt->iterator);
	if (idx < 0) {
		isl_set_free(domain);
		return 0;
	}
	int n_dim = isl_set_dim(domain, isl_dim_set);

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
