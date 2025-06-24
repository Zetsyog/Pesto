#include <cloog/isl/cloog.h>
#include <isl/set.h>
#include <isl/val.h>

#include <assert.h>

#include <pesto/clast/clast_support.h>
#include <pesto/isl_support.h>

struct clast_stmt *clast_get_stmt_before(struct clast_stmt *root, struct clast_stmt *stmt) {
	for (; root; root = root->next) {
		struct clast_stmt *syntax_next = clast_get_next(root);
		if (root->next == stmt || syntax_next == stmt) {
			return root;
		}
		if (clast_stmt_is_branch(stmt)) {
			struct clast_stmt *before = clast_get_stmt_before(syntax_next, stmt);
			if (before != NULL) {
				return before;
			}
		}
	}
	return NULL;
}

int clast_stmt_is_branch(struct clast_stmt *stmt) {
	return CLAST_STMT_IS_A(stmt, stmt_for) || CLAST_STMT_IS_A(stmt, stmt_guard) || CLAST_STMT_IS_A(stmt, stmt_block);
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

void clast_set_next(struct clast_stmt *stmt, struct clast_stmt *next) { *clast_get_next_ptr(stmt) = next; }

struct clast_stmt *clast_insert_indepth(struct clast_stmt *before, struct clast_stmt *stmt) {
	stmt->next = clast_get_next(before);
	clast_set_next(before, stmt);
	return stmt;
}

struct clast_stmt *clast_get_next(struct clast_stmt *stmt) { return *clast_get_next_ptr(stmt); }

struct clast_user_stmt *pesto_clast_create_stmt(CloogOptions *cloogOptions, CloogDomain *domain, unsigned int id) {
	CloogStatement *atiling_stmt = cloog_statement_alloc(cloogOptions->state, id);

	struct clast_user_stmt *user_stmt = new_clast_user_stmt(domain, atiling_stmt, NULL);
	cloog_statement_free(atiling_stmt);
	return user_stmt;
}

struct clast_stmt *clast_insert_after(struct clast_stmt *before, struct clast_stmt *stmt) {
	assert(before != NULL);
	if (stmt != NULL) {
		stmt->next = before->next;
	}
	before->next = stmt;
	return stmt;
}

struct clast_stmt *clast_insert_before(struct clast_stmt *root, struct clast_stmt *before, struct clast_stmt *stmt) {
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
			struct clast_stmt *inserted_stmt = clast_insert_before(deeper_stmt, before, stmt);
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

int clast_for_is_outter_tile_loop(__isl_keep isl_set *set, struct clast_for *for_stmt, int tilesize) {
	assert(for_stmt->domain != NULL);

	isl_set *domain = isl_set_copy(set);

	int n_dim = isl_set_dim(isl_set_from_cloog_domain(for_stmt->domain), isl_dim_set);
	int idx = n_dim - 1;

	// only keep constraints involving the iterator
	// domain = isl_set_drop_constraints_not_involving_dims(domain, isl_dim_set,
	// 													 idx, 1);

	isl_basic_set_list *bset_list = isl_set_get_basic_set_list(domain);
	isl_size n_bset = isl_basic_set_list_n_basic_set(bset_list);

	int is_outter_tile = 0;

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

			isl_val *val = isl_constraint_get_coefficient_val(cst, isl_dim_set, idx);

			if (isl_val_eq_si(val, tilesize) || isl_val_eq_si(val, -tilesize)) {
				is_outter_tile = 1;
			}

			isl_val_free(val);

			isl_constraint_free(cst);
			if (is_outter_tile == 1) {
				break;
			}
		}
		isl_constraint_list_free(cst_list);
		isl_basic_set_free(bset);
		if (is_outter_tile == 1) {
			break;
		}
	}

	isl_basic_set_list_free(bset_list);
	isl_set_free(domain);
	return is_outter_tile;
}

int clast_for_get_outter_tile_depth(__isl_keep isl_set *domain, struct clast_stmt *root, struct clast_for *outter_for,
									int tilesize) {
	int depth = 0;
	while ((outter_for = clast_get_parent_clast_for(root, (struct clast_stmt *)outter_for)) != NULL) {
		if (clast_for_is_outter_tile_loop(domain, outter_for, tilesize)) {
			depth += 1;
		}
	}
	return depth;
}

int clast_for_is_inner_tile_loop(struct clast_for *for_stmt, int tile_size) {
	return clast_for_get_inner_tile_outter_dim(for_stmt, tile_size) != -1;
}

int clast_for_get_inner_tile_outter_dim(struct clast_for *for_stmt, int tile_size) {
	assert(for_stmt->domain != NULL);

	isl_set *set = isl_set_from_cloog_domain(for_stmt->domain);

	isl_set *domain = isl_set_copy(set);

	int n_dim = isl_set_dim(domain, isl_dim_set);
	int idx = n_dim - 1;

	// only keep constraints involving the iterator
	domain = isl_set_drop_constraints_not_involving_dims(domain, isl_dim_set, idx, 1);

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
				isl_val *kval = isl_constraint_get_coefficient_val(cst, isl_dim_set, k);
				isl_val *idxval = isl_constraint_get_coefficient_val(cst, isl_dim_set, idx);

				if (isl_val_eq_si(idxval, 1) && isl_val_eq_si(kval, -tile_size)) {
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

struct clast_user_stmt **clast_get_user_stmt_list(struct clast_stmt *root, int *size) {
	struct clast_user_stmt **stmts = NULL;
	*size = 0;

	// DFS algorithm to traverse the syntax tree
	for (; root != NULL; root = root->next) {
		if (CLAST_STMT_IS_A(root, stmt_user)) {
			stmts = realloc(stmts, (*size + 1) * sizeof(struct clast_stmt *));
			stmts[*size] = (struct clast_user_stmt *)root;
			*size += 1;
		} else if (clast_stmt_is_branch(root)) {
			int branch_size = 0;
			struct clast_user_stmt **branch_stmts =
				clast_get_user_stmt_list(clast_branch_deeper_next(root), &branch_size);

			stmts = realloc(stmts, (*size + branch_size) * sizeof(struct clast_user_stmt *));

			for (int i = 0; i < branch_size; i += 1) {
				stmts[*size + i] = branch_stmts[i];
			}
			*size += branch_size;
			free(branch_stmts);
		}
	}

	return stmts;
}

static struct clast_for *_clast_get_parent_for(struct clast_stmt *root, struct clast_stmt *target,
											   struct clast_for *parent) {
	for (; root != NULL; root = root->next) {
		if (root == target) {
			return parent;
		}
		if (CLAST_STMT_IS_A(root, stmt_for)) {
			struct clast_for *for_stmt = (struct clast_for *)root;
			struct clast_for *result = _clast_get_parent_for(for_stmt->body, target, for_stmt);
			if (result != NULL) {
				return result;
			}
		} else if (clast_stmt_is_branch(root)) {
			struct clast_for *result = _clast_get_parent_for(clast_branch_deeper_next(root), target, parent);
			if (result != NULL) {
				return result;
			}
		}
	}
	return NULL;
}

struct clast_for *clast_get_parent_clast_for(struct clast_stmt *root, struct clast_stmt *stmt) {
	return _clast_get_parent_for(root, stmt, NULL);
}

struct clast_for *clast_get_outter_tiling_for(struct clast_stmt *root, struct clast_for *inner_for, int tile_size) {
	int outter_tile_idx = clast_for_get_inner_tile_outter_dim(inner_for, tile_size);
	if (outter_tile_idx == -1) {
		return NULL;
	}
	struct clast_for *parent = clast_get_parent_clast_for(root, (struct clast_stmt *)inner_for);
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
		for_stmt = clast_get_parent_clast_for(root, (struct clast_stmt *)for_stmt);
	}
	return depth;
}

struct clast_expr *clast_for_compute_original_bound(CloogNames *names, struct clast_for *inner_for,
													struct clast_for *outter_for, int bound) {
	isl_set *set = isl_set_copy(isl_set_from_cloog_domain(inner_for->domain));

	isl_size ndim = isl_set_dim(set, isl_dim_set);
	isl_size ndim_outter = isl_set_dim(isl_set_from_cloog_domain(outter_for->domain), isl_dim_set);

	// keep only relation of current level
	set = isl_set_drop_constraints_not_involving_dims(set, isl_dim_set, ndim - 1, 1);

	// remove constraints from the outter loop
	set = isl_set_drop_constraints_involving_dims(set, isl_dim_set, ndim_outter - 1, 1);

	set = isl_set_drop_tiling_constraints(set);
	if (bound == 0) {
		// TODO: bound min could be other than 0
		if (isl_set_dim_has_lower_bound(set, isl_dim_set, ndim - 1) == isl_bool_false) {
			set = isl_set_lower_bound_si(set, isl_dim_set, ndim - 1, 0);
		}
	}

	fprintf(stderr, "set: \n");
	isl_set_dump(set);
	fprintf(stderr, "\n");

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
			struct clast_expr *expr =
				clast_bound_from_constraint(cloog_constraint_from_isl_constraint(cst), ndim, names);

			if (result_expr == NULL) {
				result_expr = expr;
			} else {
				struct clast_reduction *red = new_clast_reduction(bound == 0 ? clast_red_max : clast_red_min, 2);
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

CloogNames *cloog_names_hard_copy(CloogNames *names) {
	CloogNames *copy = cloog_names_malloc();
	copy->nb_scalars = names->nb_scalars;
	copy->nb_scattering = names->nb_scattering;
	copy->nb_iterators = names->nb_iterators;
	copy->nb_parameters = names->nb_parameters;
	copy->references = 1;

	copy->scalars = malloc(names->nb_scalars * sizeof(char *));
	copy->scattering = malloc(names->nb_scattering * sizeof(char *));
	copy->iterators = malloc(names->nb_iterators * sizeof(char *));
	copy->parameters = malloc(names->nb_parameters * sizeof(char *));

	for (int i = 0; i < names->nb_scalars; i += 1) {
		copy->scalars[i] = strdup(names->scalars[i]);
	}
	for (int i = 0; i < names->nb_scattering; i += 1) {
		copy->scattering[i] = strdup(names->scattering[i]);
	}
	for (int i = 0; i < names->nb_iterators; i += 1) {
		copy->iterators[i] = strdup(names->iterators[i]);
	}
	for (int i = 0; i < names->nb_parameters; i += 1) {
		copy->parameters[i] = strdup(names->parameters[i]);
	}

	return copy;
}

int clast_annotate_loops(osl_scop_p program, struct clast_stmt *root) {

	int j, nclastloops, nclaststmts;
	struct clast_for **clastloops = NULL;
	int *claststmts = NULL;
	int ret = 0;

	if (program == NULL) {
		return ret;
	}

	osl_loop_p ll = osl_generic_lookup(program->extension, OSL_URI_LOOP);
	while (ll) {
		// for each loop
		osl_loop_p loop = ll;
		ClastFilter filter = {loop->iter, loop->stmt_ids, loop->nb_stmts, subset};

		clast_filter(root, filter, &clastloops, &nclastloops, &claststmts, &nclaststmts);

		if (claststmts) {
			free(claststmts);
			claststmts = NULL;
		}

		/* There should be at least one */
		if (nclastloops == 0) { // FROM PLUTO
			/* Sometimes loops may disappear (1) tile size larger than trip
			 * count 2) it's a scalar dimension but can't be determined from the
			 * trans matrix */
			printf("Warning: parallel poly loop not found in AST\n");
			ll = ll->next;
			continue;
		}
		for (j = 0; j < nclastloops; j++) {

			if (loop->directive & CLAST_PARALLEL_VEC) {
				clastloops[j]->parallel |= CLAST_PARALLEL_VEC;
				ret |= CLAST_PARALLEL_VEC;
			}

			if (loop->directive & CLAST_PARALLEL_OMP) {
				clastloops[j]->parallel |= CLAST_PARALLEL_OMP;
				ret |= CLAST_PARALLEL_OMP;
				if (loop->private_vars && strlen(loop->private_vars) > 0) {
					clastloops[j]->private_vars = strdup(loop->private_vars);
				} else {
					clastloops[j]->private_vars = NULL;
				}
			}

			if (loop->directive & CLAST_PARALLEL_USER) {
				clastloops[j]->parallel |= CLAST_PARALLEL_USER;
				ret |= CLAST_PARALLEL_USER;
				if (loop->user) {
					clastloops[j]->user_directive = strdup(loop->user);
				}
			}
		}

		if (clastloops) {
			free(clastloops);
			clastloops = NULL;
		}

		ll = ll->next;
	}

	return ret;
}

struct clast_expr *clast_bound_from_isl_set(CloogNames *names, __isl_keep isl_set *set, int bound, int dim) {
	isl_size ndim = isl_set_dim(set, isl_dim_set);

	assert(dim < ndim && "dim should be less than the number of dimensions");

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

		if (bound_fn(cst, isl_dim_set, dim)) {
			struct clast_expr *expr =
				clast_bound_from_constraint(cloog_constraint_from_isl_constraint(cst), ndim, names);

			if (result_expr == NULL) {
				result_expr = expr;
			} else {
				struct clast_reduction *red = new_clast_reduction(bound == 0 ? clast_red_max : clast_red_min, 2);
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

	return result_expr;
}

struct clast_for *clast_find_outter_tiling_for(struct clast_stmt *root, struct clast_for *inner_for) {
	isl_set *inner_domain = isl_set_copy(isl_set_from_cloog_domain(inner_for->domain));
	isl_size inner_ndim = isl_set_dim(inner_domain, isl_dim_set);

	inner_domain = isl_set_drop_constraints_not_involving_dims(inner_domain, isl_dim_set, inner_ndim - 1, 1);

	// iterate over the parent loops in ast
	for (struct clast_for *outter_for = clast_get_parent_clast_for(root, &(inner_for->stmt)); outter_for != NULL;
		 outter_for = clast_get_parent_clast_for(root, (struct clast_stmt *)outter_for)) {
		isl_set *outter_domain = isl_set_copy(isl_set_from_cloog_domain(outter_for->domain));
		isl_size outter_ndim = isl_set_dim(outter_domain, isl_dim_set);

		// if this iterator is not a tiling loop, skip it
		if (isl_set_is_tiling_dim(inner_domain, outter_ndim - 1) == isl_bool_false) {
			isl_set_free(outter_domain);
			continue;
		}
		// this set contains only the constraints of the inner loop iterator and of the outter loop iterator
		isl_set *outter_constraints =
			isl_set_drop_constraints_not_involving_dims(isl_set_copy(inner_domain), isl_dim_set, outter_ndim - 1, 1);

		if (isl_set_is_tiling_dim(outter_constraints, outter_ndim - 1) == isl_bool_true) {
			// if it is still a tiling loop, we found the outter loop
			isl_set_free(outter_constraints);
			isl_set_free(inner_domain);
			isl_set_free(outter_domain);
			return outter_for;
		}

		isl_set_free(outter_constraints);

		isl_set_free(outter_domain);
	}

	isl_set_free(inner_domain);

	return NULL;
}

void clast_replace_stmt(struct clast_stmt *root, struct clast_stmt *old_stmt, struct clast_stmt *new_stmt) {
	assert(root != NULL);
	assert(old_stmt != NULL);
	assert(new_stmt != NULL);

	struct clast_stmt *before = clast_get_stmt_before(root, old_stmt);
	assert(before != NULL && "The statement should be in the syntax tree");

	if (before->next == old_stmt) {
		before->next = new_stmt;
		return;
	}

	if (CLAST_STMT_IS_A(before, stmt_for)) {
		((struct clast_for *)before)->body = new_stmt;
	} else if (CLAST_STMT_IS_A(before, stmt_guard)) {
		((struct clast_guard *)before)->then = new_stmt;
	} else if (CLAST_STMT_IS_A(before, stmt_block)) {
		((struct clast_block *)before)->body = new_stmt;
	} else {
		fprintf(stderr, "clast_replace_stmt: unsupported statement type\n");
		assert(0 && "Unsupported statement type");
		return;
	}
}

struct clast_for *clast_insert_enclosing_for(struct clast_stmt *root, struct clast_stmt *stmt, CloogDomain *domain) {
	assert(root != NULL);
	assert(stmt != NULL);

	struct clast_stmt *before = clast_get_stmt_before(root, stmt);

	assert(before != NULL && "The statement should be in the syntax tree");

	struct clast_for *enclosing_for = new_clast_for(domain, "hey", NULL, NULL, NULL);
	enclosing_for->body = stmt;

	clast_replace_stmt(root, stmt, &(enclosing_for->stmt));

	return enclosing_for;
}

struct clast_for *clast_get_innermost_for(struct clast_stmt *root_for) {
	assert(root_for != NULL);

	struct clast_for *innermost_for = NULL;
	for (; root_for != NULL; root_for = clast_branch_deeper_next(root_for)) {
		if (CLAST_STMT_IS_A(root_for, stmt_for)) {
			innermost_for = (struct clast_for *)root_for;
		}
	}

	return innermost_for;
}

/**
 * Construct a (deep) copy of an expression clast.
 * from cloog/source/clast.c
 */
static struct clast_expr *clast_expr_hard_copy(struct clast_expr *e) {
	if (!e)
		return NULL;

	switch (e->type) {
	case clast_expr_name: {
		struct clast_name *n = (struct clast_name *)e;
		return &new_clast_name(n->name)->expr;
	}
	case clast_expr_term: {
		struct clast_term *t = (struct clast_term *)e;
		return &new_clast_term(t->val, clast_expr_hard_copy(t->var))->expr;
	}
	case clast_expr_red: {
		int i;
		struct clast_reduction *r = (struct clast_reduction *)e;
		struct clast_reduction *r2 = new_clast_reduction(r->type, r->n);
		for (i = 0; i < r->n; ++i)
			r2->elts[i] = clast_expr_hard_copy(r->elts[i]);
		return &r2->expr;
	}
	case clast_expr_bin: {
		struct clast_binary *b = (struct clast_binary *)e;
		return &new_clast_binary(b->type, clast_expr_hard_copy(b->LHS), b->RHS)->expr;
	}
	default:
		assert(0);
	}
}

static struct clast_for *clast_for_hard_copy(struct clast_for *for_stmt) {
	assert(for_stmt != NULL);
	struct clast_expr *LB = clast_expr_hard_copy(for_stmt->LB);
	struct clast_expr *UB = clast_expr_hard_copy(for_stmt->UB);
	struct clast_for *copy = new_clast_for(for_stmt->domain, for_stmt->iterator, LB, UB, NULL);
	cloog_int_set(copy->stride, for_stmt->stride);
	copy->body = clast_stmt_hard_copy(for_stmt->body);
	copy->parallel = for_stmt->parallel;

	if (for_stmt->private_vars != NULL) {
		copy->private_vars = strdup(for_stmt->private_vars);
	} else {
		copy->private_vars = NULL;
	}

	if (for_stmt->reduction_vars != NULL) {
		copy->reduction_vars = strdup(for_stmt->reduction_vars);
	} else {
		copy->reduction_vars = NULL;
	}

	if (for_stmt->time_var_name != NULL) {
		copy->time_var_name = strdup(for_stmt->time_var_name);
	} else {
		copy->time_var_name = NULL;
	}

	if (for_stmt->user_directive != NULL) {
		copy->user_directive = strdup(for_stmt->user_directive);
	} else {
		copy->user_directive = NULL;
	}

	return copy;
}

static struct clast_block *clast_block_hard_copy(struct clast_block *block_stmt) {
	assert(block_stmt != NULL);

	struct clast_block *copy = new_clast_block();
	copy->body = clast_stmt_hard_copy(block_stmt->body);
	return copy;
}

static struct clast_root *clast_root_hard_copy(struct clast_root *root_stmt) {
	assert(root_stmt != NULL);

	struct clast_root *copy = new_clast_root(cloog_names_hard_copy(root_stmt->names));

	return copy;
}

static struct clast_user_stmt *clast_user_stmt_hard_copy(struct clast_user_stmt *user_stmt) {
	assert(user_stmt != NULL);

	struct clast_stmt *subs = NULL;
	if (user_stmt->substitutions != NULL) {
		subs = clast_stmt_hard_copy(user_stmt->substitutions);
	}

	struct clast_user_stmt *copy = new_clast_user_stmt(user_stmt->domain, user_stmt->statement, subs);

	return copy;
}

static struct clast_guard *clast_guard_hard_copy(struct clast_guard *guard_stmt) {
	assert(guard_stmt != NULL);

	struct clast_guard *copy = new_clast_guard(guard_stmt->n);
	copy->then = clast_stmt_hard_copy(guard_stmt->then);
	for (int i = 0; i < guard_stmt->n; i += 1) {
		copy->eq[i].LHS = clast_expr_hard_copy(guard_stmt->eq[i].LHS);
		copy->eq[i].RHS = clast_expr_hard_copy(guard_stmt->eq[i].RHS);
		copy->eq[i].sign = guard_stmt->eq[i].sign;
	}
	return copy;
}

static struct clast_assignment *clast_assignment_hard_copy(struct clast_assignment *assignment_stmt) {
	assert(assignment_stmt != NULL);

	char *LHS = assignment_stmt->LHS ? strdup(assignment_stmt->LHS) : NULL;

	struct clast_assignment *copy = new_clast_assignment(LHS, clast_expr_hard_copy(assignment_stmt->RHS));
	return copy;
}

struct clast_stmt *clast_stmt_hard_copy(struct clast_stmt *stmt) {
	fprintf(stderr, "PTR %p\n", stmt);
	assert(stmt != NULL);

	struct clast_stmt *result_root = NULL;
	struct clast_stmt *it = NULL;

	for (struct clast_stmt *stmt_it = stmt; stmt_it != NULL; stmt_it = stmt_it->next) {
		struct clast_stmt *copy = NULL;
		if (CLAST_STMT_IS_A(stmt_it, stmt_root)) {
			fprintf(stderr, "Copying root\n");
			copy = &(clast_root_hard_copy((struct clast_root *)stmt_it)->stmt);
		} else if (CLAST_STMT_IS_A(stmt_it, stmt_ass)) {
			fprintf(stderr, "Copying ass\n");
			copy = &(clast_assignment_hard_copy((struct clast_assignment *)stmt_it)->stmt);
		} else if (CLAST_STMT_IS_A(stmt_it, stmt_user)) {
			fprintf(stderr, "Copying user\n");
			copy = &(clast_user_stmt_hard_copy((struct clast_user_stmt *)stmt_it)->stmt);
		} else if (CLAST_STMT_IS_A(stmt_it, stmt_block)) {
			fprintf(stderr, "Copying block\n");
			copy = &(clast_block_hard_copy((struct clast_block *)stmt_it)->stmt);
		} else if (CLAST_STMT_IS_A(stmt_it, stmt_for)) {
			fprintf(stderr, "Copying for\n");
			copy = &(clast_for_hard_copy((struct clast_for *)stmt_it)->stmt);
		} else if (CLAST_STMT_IS_A(stmt_it, stmt_guard)) {
			fprintf(stderr, "Copying guard\n");
			copy = &(clast_guard_hard_copy((struct clast_guard *)stmt_it)->stmt);
		}

		if (copy == NULL) {
			fprintf(stderr, "Unknown statement type\n");
			return NULL;
		}

		copy->next = NULL;
		if (result_root == NULL) {
			result_root = copy;
			it = copy;
		} else {
			it->next = copy;
			it = it->next;
		}
	}

	return result_root;
}