#include <isl/aff.h>
#include <isl/ctx.h>
#include <isl/id.h>
#include <isl/ilp.h>
#include <isl/map.h>
#include <isl/printer.h>
#include <isl/set.h>
#include <isl/space.h>
#include <isl/union_map.h>
#include <isl/union_set.h>
#include <isl/val.h>
#include <isl/val_gmp.h>

#include <osl/osl.h>

#include <assert.h>

#include "isl_support.h"

void osl_scop_to_isl(osl_scop_p scop, isl_ctx *ctx, isl_union_map **schedules,
					 isl_union_map **reads, isl_union_map **writes) {
	if (scop == NULL) {
		return;
	}
	*reads = NULL;
	*writes = NULL;

	int n_param = osl_scop_get_nb_parameters(scop);

	isl_union_set *domains = isl_union_set_empty_ctx(ctx);
	isl_union_map *_schedules = isl_union_map_empty_ctx(ctx);
	isl_union_map *_reads = isl_union_map_empty_ctx(ctx);
	isl_union_map *_writes = isl_union_map_empty_ctx(ctx);
	osl_strings_p names = (osl_strings_p)scop->parameters->data;

	isl_space *param_space = isl_space_params_alloc(ctx, n_param);

	int i = 0;
	for (osl_statement_p stmt = scop->statement; stmt != NULL;
		 stmt = stmt->next, i += 1) {

		char stmt_name[100] = {0};
		sprintf(stmt_name, "S_%d", i);
		isl_id *id = isl_id_alloc(ctx, stmt_name, NULL);

		isl_space *space = isl_space_add_named_tuple_id_ui(
			isl_space_copy(param_space), id, stmt->domain->nb_input_dims);

		isl_set *set = isl_set_from_osl_relation(stmt->domain, ctx);

		for (int i = 0; i < n_param; i++) {
			set = isl_set_set_dim_name(set, isl_dim_param, i, names->string[i]);
		}
		domains = isl_union_set_add_set(domains, set);

		isl_map *map = isl_map_from_osl_relation(stmt->scattering, ctx);
		for (int i = 0; i < n_param; i++) {
			map = isl_map_set_dim_name(map, isl_dim_param, i, names->string[i]);
		}
		_schedules = isl_union_map_add_map(_schedules, map);

		for (osl_relation_list_p node = stmt->access; node != NULL;
			 node = node->next) {
			osl_relation_p rel = node->elt;
			if (rel->type == OSL_TYPE_READ) {
				map = isl_map_from_osl_relation(rel, ctx);
				_reads = isl_union_map_add_map(_reads, map);
			}
			if (rel->type == OSL_TYPE_WRITE) {
				map = isl_map_from_osl_relation(rel, ctx);
				_writes = isl_union_map_add_map(_writes, map);
			}
		}
	}
	_schedules = isl_union_map_intersect_domain(_schedules, domains);

	*schedules = _schedules;
	*reads = _reads;
	*writes = _writes;
}
__isl_give isl_set *isl_set_from_osl_relation(osl_relation_p relation,
											  isl_ctx *ctx) {
	char *str = osl_relation_spprint_polylib(relation, NULL);
	isl_set *set = isl_set_read_from_str(ctx, str);
	free(str);

	return set;
}

__isl_give isl_map *isl_map_from_osl_relation(osl_relation_p relation,
											  isl_ctx *ctx) {
	char *str = osl_relation_spprint_polylib(relation, NULL);
	isl_map *map = isl_map_read_from_str(ctx, str);
	free(str);
	return map;
}

__isl_give isl_constraint *
isl_basic_set_dim_lb_cst(__isl_keep isl_basic_set *bset, int pos) {
	// retrieve constraint list
	isl_constraint_list *cst_list = isl_basic_set_get_constraint_list(bset);
	isl_size list_len = isl_constraint_list_size(cst_list);
	isl_constraint *result = NULL;
	for (isl_size i = 0; i < list_len; i++) {
		isl_constraint *cst = isl_constraint_list_get_at(cst_list, i);
		if (isl_constraint_is_lower_bound(cst, isl_dim_set, pos)) {
			result = cst;
			break;
		}
		isl_constraint_free(cst);
	}

	isl_constraint_list_free(cst_list);

	return result;
}

__isl_give isl_constraint *
isl_basic_set_dim_ub_cst(__isl_keep isl_basic_set *bset, int pos) {
	// retrieve constraint list
	isl_constraint_list *cst_list = isl_basic_set_get_constraint_list(bset);
	isl_size list_len = isl_constraint_list_size(cst_list);
	isl_constraint *result = NULL;
	for (isl_size i = 0; i < list_len; i++) {
		isl_constraint *cst = isl_constraint_list_get_at(cst_list, i);
		if (isl_constraint_is_upper_bound(cst, isl_dim_set, pos)) {
			result = cst;
			break;
		}
		isl_constraint_free(cst);
	}

	isl_constraint_list_free(cst_list);

	return result;
}

__isl_give isl_basic_set *
isl_basic_set_minmax_union(__isl_take isl_basic_set *bset1,
						   __isl_take isl_basic_set *bset2) {
	isl_size ndim1 = isl_basic_set_n_dim(bset1);
	isl_size ndim2 = isl_basic_set_n_dim(bset2);

	assert(ndim1 == ndim2);
	isl_basic_set *result =
		isl_basic_set_universe(isl_basic_set_get_space(bset1));

	for (isl_size i = 0; i < ndim1; i++) {
		isl_constraint *lb1 = isl_basic_set_dim_lb_cst(bset1, i);
		isl_constraint *lb2 = isl_basic_set_dim_lb_cst(bset2, i);
		isl_constraint *ub1 = isl_basic_set_dim_ub_cst(bset1, i);
		isl_constraint *ub2 = isl_basic_set_dim_ub_cst(bset2, i);

		if (lb1 == NULL && lb2 == NULL) {
			// no lower bound
			// no constraint
		} else if (lb1 == NULL) {
			// no lower bound in bset1
			// add lower bound of bset2
			result =
				isl_basic_set_add_constraint(result, isl_constraint_copy(lb2));
		} else if (lb2 == NULL) {
			// no lower bound in bset2
			// add lower bound of bset1
			result =
				isl_basic_set_add_constraint(result, isl_constraint_copy(lb1));
		} else {
			isl_pw_aff *aff1 = isl_pw_aff_from_aff(isl_constraint_get_aff(lb1));
			isl_pw_aff *aff2 = isl_pw_aff_from_aff(isl_constraint_get_aff(lb2));

			int cmp = isl_pw_aff_plain_cmp(aff1, aff2);

			isl_pw_aff_free(aff1);
			isl_pw_aff_free(aff2);

			if (cmp < 0) {
				// lb1 < lb2
				result = isl_basic_set_add_constraint(result,
													  isl_constraint_copy(lb2));
			} else {
				result = isl_basic_set_add_constraint(result,
													  isl_constraint_copy(lb1));
			}
		}

		if (ub1 == NULL && ub2 == NULL) {
			// no upper bound
			// no constraint
		} else if (ub1 == NULL) {
			// no upper bound in bset1
			// add upper bound of bset2
			result =
				isl_basic_set_add_constraint(result, isl_constraint_copy(ub2));
		} else if (ub2 == NULL) {
			// no upper bound in bset2
			// add upper bound of bset1
			result =
				isl_basic_set_add_constraint(result, isl_constraint_copy(ub1));
		} else {
			isl_pw_aff *aff1 = isl_pw_aff_from_aff(isl_constraint_get_aff(ub1));
			isl_pw_aff *aff2 = isl_pw_aff_from_aff(isl_constraint_get_aff(ub2));

			int cmp = isl_pw_aff_plain_cmp(aff1, aff2);

			isl_pw_aff_free(aff1);
			isl_pw_aff_free(aff2);
			if (cmp < 0) {
				// ub1 < ub2
				result = isl_basic_set_add_constraint(result,
													  isl_constraint_copy(ub2));
			} else {
				result = isl_basic_set_add_constraint(result,
													  isl_constraint_copy(ub1));
			}
		}

		isl_constraint_free(lb1);
		isl_constraint_free(lb2);
		isl_constraint_free(ub1);
		isl_constraint_free(ub2);
	}

	isl_basic_set_free(bset1);
	isl_basic_set_free(bset2);

	return result;
}

__isl_give isl_set *isl_set_from_osl_stmt(isl_ctx *ctx, osl_statement_p stmt) {
	isl_set *domain = isl_set_from_osl_relation(stmt->domain, ctx);
	isl_map *scattering = isl_map_from_osl_relation(stmt->scattering, ctx);
	domain = isl_set_apply(domain, scattering);

	return domain;
}

__isl_give isl_basic_set *isl_basic_set_from_osl_scop(isl_ctx *ctx,
													  osl_scop_p scop) {
	osl_scop_p tmp_scop = osl_scop_remove_unions(scop);
	osl_scop_normalize_scattering(tmp_scop);

	isl_basic_set *result_bset = NULL;

	for (osl_statement_p stmt = scop->statement; stmt != NULL;
		 stmt = stmt->next) {
		isl_set *domain = isl_set_from_osl_stmt(ctx, stmt);
		isl_basic_set_list *bset_list = isl_set_get_basic_set_list(domain);
		isl_size n_bset = isl_basic_set_list_n_basic_set(bset_list);

		if (n_bset == 0) {
			continue;
		}
		isl_basic_set *bset = isl_basic_set_list_get_at(bset_list, 0);

		for (int i = 1; i < n_bset; i++) {
			isl_basic_set *tmp_bset = isl_basic_set_list_get_at(bset_list, i);
			bset = isl_basic_set_minmax_union(bset, tmp_bset);
		}

		isl_basic_set_list_free(bset_list);
		isl_set_free(domain);

		if (result_bset == NULL) {
			result_bset = bset;
		} else {
			result_bset = isl_basic_set_minmax_union(result_bset, bset);
		}
	}

	int npar = osl_scop_get_nb_parameters(tmp_scop);
	osl_strings_p names = (osl_strings_p)tmp_scop->parameters->data;

	for (int i = 0; i < npar; i++) {
		result_bset = isl_basic_set_set_dim_name(result_bset, isl_dim_param, i,
												 names->string[i]);
	}

	osl_scop_free(tmp_scop);
	return result_bset;
}

void get_lb_ub_expr_from_isl(isl_basic_set *bset, int pos,
							 const char *lb_prefix, const char *ub_prefix,
							 char **lb_expr, char **ub_expr) {
	isl_ctx *ctx = isl_basic_set_get_ctx(bset);

	isl_size ndim = isl_basic_set_dim(bset, isl_dim_set);

	isl_constraint_list *cst_list = isl_basic_set_get_constraint_list(bset);
}

__isl_give isl_set *
isl_set_set_param_names_from_osl_scop(__isl_take isl_set *set,
									  osl_scop_p scop) {
	// set param names
	for (int i = 0; i < osl_scop_get_nb_parameters(scop); i += 1) {
		set = isl_set_set_dim_name(
			set, isl_dim_param, i,
			((osl_strings_p)scop->parameters->data)->string[i]);
	}

	return set;
}

int isl_set_count_tiling_dimensions(__isl_keep isl_set *set) {
	int count = 0;
	int ndim = isl_set_dim(set, isl_dim_set);

	isl_basic_set_list *bset_list = isl_set_get_basic_set_list(set);
	isl_basic_set *bset = isl_basic_set_list_get_at(bset_list, 0);

	for (int i = 0; i < ndim; i += 1) {
		if (isl_basic_set_is_tiling_dim(bset, i)) {
			count += 1;
		}
	}

	isl_basic_set_free(bset);
	isl_basic_set_list_free(bset_list);
	return count;
}

isl_bool isl_basic_set_is_tiling_dim(__isl_keep isl_basic_set *bset, int pos) {
	isl_ctx *ctx = isl_basic_set_get_ctx(bset);

	isl_constraint_list *cst_list = isl_basic_set_get_constraint_list(bset);
	isl_size list_len = isl_constraint_list_size(cst_list);
	isl_bool result = isl_bool_false;
	isl_val *cmp = isl_val_int_from_ui(ctx, 32);

	for (isl_size i = 0; i < list_len; i++) {
		isl_constraint *cst = isl_constraint_list_get_at(cst_list, i);
		isl_val *val =
			isl_constraint_get_coefficient_val(cst, isl_dim_set, pos);
		if (isl_val_abs_eq(val, cmp)) {
			result = isl_bool_true;
			i = list_len;
		}
		isl_val_free(val);
		isl_constraint_free(cst);
	}

	isl_val_free(cmp);
	isl_constraint_list_free(cst_list);

	return result;
}

__isl_give isl_set *isl_set_remove_tiling_dimensions(__isl_take isl_set *set) {
	while (isl_set_count_tiling_dimensions(set) > 0) {
		isl_size ndim = isl_set_dim(set, isl_dim_set);
		isl_basic_set_list *bset_list = isl_set_get_basic_set_list(set);

		for (int i = 0; i < ndim; i += 1) {
			int basic_set_count = isl_basic_set_list_n_basic_set(bset_list);
			for (int j = 0; j < basic_set_count; j += 1) {
				isl_basic_set *bset = isl_basic_set_list_get_at(bset_list, j);
				if (isl_basic_set_is_tiling_dim(bset, i)) {
					set = isl_set_remove_dims(set, isl_dim_set, i, 1);
					j = basic_set_count;
					i = ndim;
				}
				isl_basic_set_free(bset);
			}
		}
		isl_basic_set_list_free(bset_list);
	}

	return set;
}

__isl_give isl_set *isl_set_drop_tiling_constraints(__isl_take isl_set *set) {
	while (isl_set_count_tiling_dimensions(set) > 0) {
		isl_size ndim = isl_set_dim(set, isl_dim_set);
		isl_basic_set_list *bset_list = isl_set_get_basic_set_list(set);

		for (int i = 0; i < ndim; i += 1) {
			int basic_set_count = isl_basic_set_list_n_basic_set(bset_list);
			for (int j = 0; j < basic_set_count; j += 1) {
				isl_basic_set *bset = isl_basic_set_list_get_at(bset_list, j);
				if (isl_basic_set_is_tiling_dim(bset, i)) {
					set = isl_set_drop_constraints_involving_dims(
						set, isl_dim_set, i, 1);
					j = basic_set_count;
					i = ndim;
				}
				isl_basic_set_free(bset);
			}
		}
		isl_basic_set_list_free(bset_list);
	}

	return set;
}