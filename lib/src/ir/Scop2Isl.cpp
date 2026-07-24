/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <isl/ctx.h>
#include <isl/map.h>
#include <isl/mat.h>
#include <isl/set.h>
#include <isl/space.h>
#include <isl/union_map.h>
#include <isl/union_set.h>
#include <isl/val.h>

#include <osl/osl.h>

#include <pesto/ir/Scop2Isl.hpp>

using pesto::ir::converter::Scop2Isl;

/* FROM osl_pluto.c */
static osl_names_p get_scop_names(osl_scop_p scop) {

	// generate temp names
	osl_names_p names = osl_scop_names(scop);

	// if scop has names substitute them for temp names
	if (scop->context->nb_parameters) {
		osl_strings_free(names->parameters);
		names->parameters = osl_strings_clone((osl_strings_p)scop->parameters->data);
	}

	osl_arrays_p arrays = (osl_arrays_p)osl_generic_lookup(scop->extension, OSL_URI_ARRAYS);
	if (arrays) {
		osl_strings_free(names->arrays);
		names->arrays = osl_arrays_to_strings(arrays);
	}

	return names;
}
/*
 * In an [eq -I A c] relation, rows can be ordered any way.
 * Returns the index for the row for the nth output dimension.
 */
static int osl_relation_get_row_id_for_nth_dimension(osl_relation_p relation, int ndim) {
	int nb_ndims_found = 0;
	int row_id = -1;

	if (relation == NULL)
		return OSL_UNDEFINED;

	if ((relation->nb_rows < ndim) || (0 > ndim)) {
		fprintf(stderr, "error: dimension out of bounds");
		exit(1);
	}

	nb_ndims_found = 0;
	for (int i = 0; i < relation->nb_rows; i++) {
		if (!osl_int_zero(relation->precision, relation->m[i][ndim])) {
			nb_ndims_found++;
			row_id = i;
		}
	}
	if (nb_ndims_found == 0) {
		fprintf(stderr, "error: dimension %d not found\n", ndim);
		exit(1);
	}
	if (nb_ndims_found > 1) {
		fprintf(stderr, "error: dimension occurs multiple times\n");
		exit(1);
	}

	return row_id;
}

/* Convert an m x (1 + m + n + 1) osl_relation_p [d -I A c]
 * to an m x (m + n + 1) isl_mat [-I A c].
 */
static __isl_give isl_mat *extract_equalities_osl_access(isl_ctx *ctx, osl_relation_p relation) {
	int i, j;
	int n_col, n_row;
	isl_mat *eq;

	n_row = relation->nb_rows == 1 ? 1 : relation->nb_rows - 1;
	n_col = relation->nb_columns - (relation->nb_rows == 1 ? 1 : 2);

	eq = isl_mat_alloc(ctx, n_row, n_col);

	if (relation->nb_rows == 1) {
		isl_val *v = isl_val_negone(ctx);
		eq = isl_mat_set_element_val(eq, 0, 0, v);
		for (j = 1; j < n_col; ++j) {
			v = isl_val_zero(ctx);
			eq = isl_mat_set_element_val(eq, 0, j, v);
		}
	} else {
		for (i = 1; i < relation->nb_rows; ++i) {
			for (j = 2; j < relation->nb_columns; ++j) {
				int row = osl_relation_get_row_id_for_nth_dimension(relation, i + 1);
				int t = osl_int_get_si(relation->precision, relation->m[row][j]);
				isl_val *v = isl_val_int_from_si(ctx, t);
				eq = isl_mat_set_element_val(eq, i - 1, j - 2, v);
			}
		}
	}

	return eq;
}

/*
 * Like osl_access_list_to_isl_union_map, but just for a single osl access
 * (read or write)
 */
static __isl_give isl_map *osl_basic_access_to_isl_map(osl_relation_p access, __isl_take isl_set *dom, char **arrays) {
	int len, n_col;
	isl_ctx *ctx;
	isl_space *dim;
	isl_mat *eq, *ineq;

	ctx = isl_set_get_ctx(dom);

	n_col = access->nb_columns - (access->nb_rows == 1 ? 1 : 2);
	len = access->nb_rows == 1 ? 1 : access->nb_rows - 1;

	isl_basic_map *bmap;
	isl_map *map;
	int arr = osl_relation_get_array_id(access) - 1;

	dim = isl_set_get_space(dom);
	dim = isl_space_from_domain(dim);
	dim = isl_space_add_dims(dim, isl_dim_out, len);
	dim = isl_space_set_tuple_name(dim, isl_dim_out, arrays[arr]);

	ineq = isl_mat_alloc(ctx, 0, n_col);
	eq = extract_equalities_osl_access(ctx, access);

	bmap = isl_basic_map_from_constraint_matrices(dim, eq, ineq, isl_dim_out, isl_dim_in, isl_dim_div, isl_dim_param,
												  isl_dim_cst);
	map = isl_map_from_basic_map(bmap);
	map = isl_map_intersect_domain(map, dom);

	return map;
}
/* Convert a osl_relation_p containing the constraints of a domain
 * to an isl_set.
 * One shot only; does not take into account the next ptr.
 */
static __isl_give isl_set *osl_relation_to_isl_set(osl_relation_p relation, __isl_take isl_space *dim) {
	int i, j;
	int n_eq = 0, n_ineq = 0;
	isl_ctx *ctx;
	isl_mat *eq, *ineq;
	isl_basic_set *bset;

	ctx = isl_space_get_ctx(dim);

	for (i = 0; i < relation->nb_rows; ++i)
		if (osl_int_zero(relation->precision, relation->m[i][0]))
			n_eq++;
		else
			n_ineq++;

	eq = isl_mat_alloc(ctx, n_eq, relation->nb_columns - 1);
	ineq = isl_mat_alloc(ctx, n_ineq, relation->nb_columns - 1);

	n_eq = n_ineq = 0;
	for (i = 0; i < relation->nb_rows; ++i) {
		isl_mat **m;
		int row;

		if (osl_int_zero(relation->precision, relation->m[i][0])) {
			m = &eq;
			row = n_eq++;
		} else {
			m = &ineq;
			row = n_ineq++;
		}

		for (j = 0; j < relation->nb_columns - 1; ++j) {
			int t = osl_int_get_si(relation->precision, relation->m[i][1 + j]);
			*m = isl_mat_set_element_si(*m, row, j, t);
		}
	}

	bset = isl_basic_set_from_constraint_matrices(dim, eq, ineq, isl_dim_set, isl_dim_div, isl_dim_param, isl_dim_cst);
	return isl_set_from_basic_set(bset);
}
/* Convert a osl_relation_p describing a union of domains
 * to an isl_set.
 */
static __isl_give isl_set *osl_relation_list_to_isl_set(osl_relation_p list, __isl_take isl_space *space) {
	isl_set *set;

	set = isl_set_empty(isl_space_copy(space));
	for (; list; list = list->next) {
		isl_set *set_i;
		set_i = osl_relation_to_isl_set(list, isl_space_copy(space));
		set = isl_set_union(set, set_i);
	}

	isl_space_free(space);
	return set;
}

/* Convert an m x (1 + m + n + 1) osl_relation_p [d -I A c]
 * to an m x (m + n + 1) isl_mat [-I A c].
 */
static __isl_give isl_mat *extract_equalities_osl(isl_ctx *ctx, osl_relation_p relation) {
	int i, j;
	int n_col, n_row;
	isl_mat *eq;

	n_col = relation->nb_columns;
	n_row = relation->nb_rows;

	eq = isl_mat_alloc(ctx, n_row, n_col - 1);

	for (i = 0; i < n_row; ++i) {
		for (j = 0; j < n_col - 1; ++j) {
			int row = osl_relation_get_row_id_for_nth_dimension(relation, i + 1);
			int t = osl_int_get_si(relation->precision, relation->m[row][1 + j]);
			isl_val *v = isl_val_int_from_si(ctx, t);
			eq = isl_mat_set_element_val(eq, i, j, v);
		}
	}

	return eq;
}

/* Convert a osl_relation_p scattering [0 M A c] to
 * the isl_map { i -> A i + c } in the space prescribed by "dim".
 */
static __isl_give isl_map *osl_scattering_to_isl_map(osl_relation_p scattering, __isl_take isl_space *dim) {
	int n_col;
	isl_ctx *ctx;
	isl_mat *eq, *ineq;
	isl_basic_map *bmap;

	ctx = isl_space_get_ctx(dim);
	n_col = scattering->nb_columns;

	ineq = isl_mat_alloc(ctx, 0, n_col - 1);
	eq = extract_equalities_osl(ctx, scattering);

	bmap = isl_basic_map_from_constraint_matrices(dim, eq, ineq, isl_dim_out, isl_dim_in, isl_dim_div, isl_dim_param,
												  isl_dim_cst);

	return isl_map_from_basic_map(bmap);
}

/* Convert a osl_relation_list_p describing a series of accesses [eq -I B c]
 * to an isl_union_map with domain "dom" (in space "D").
 * The -I columns identify the output dimensions of the access, the first
 * of them being the identity of the array being accessed.  The remaining
 * output dimensions identiy the array subscripts.
 *
 * Let "A" be array identified by the first entry.
 * The input dimension columns have the form [B c].
 * Each such access is converted to a map { D[i] -> A[B i + c] } * dom.
 *
 */
static __isl_give isl_union_map *osl_access_list_to_isl_union_map(osl_relation_list_p list, __isl_take isl_set *dom,
																  char **arrays) {
	int len, n_col;
	isl_ctx *ctx;
	isl_space *space;
	isl_mat *eq, *ineq;
	isl_union_map *res;

	ctx = isl_set_get_ctx(dom);

	space = isl_set_get_space(dom);
	space = isl_space_drop_dims(space, isl_dim_set, 0, isl_space_dim(space, isl_dim_set));
	res = isl_union_map_empty(space);

	for (; list; list = list->next) {

		n_col = list->elt->nb_columns - (list->elt->nb_rows == 1 ? 1 : 2);
		len = list->elt->nb_rows == 1 ? 1 : list->elt->nb_rows - 1;

		isl_basic_map *bmap;
		isl_map *map;
		int arr = osl_relation_get_array_id(list->elt) - 1;

		space = isl_set_get_space(dom);
		space = isl_space_from_domain(space);
		space = isl_space_add_dims(space, isl_dim_out, len);
		space = isl_space_set_tuple_name(space, isl_dim_out, arrays[arr]);

		ineq = isl_mat_alloc(ctx, 0, n_col);
		eq = extract_equalities_osl_access(ctx, list->elt);

		bmap = isl_basic_map_from_constraint_matrices(space, eq, ineq, isl_dim_out, isl_dim_in, isl_dim_div,
													  isl_dim_param, isl_dim_cst);
		map = isl_map_from_basic_map(bmap);
		map = isl_map_intersect_domain(map, isl_set_copy(dom));
		res = isl_union_map_union(res, isl_union_map_from_map(map));
	}

	isl_set_free(dom);

	return res;
}

/*
 * Checks if a row in an OSL relation has local dimensions
 */
static int osl_relation_row_has_local_dim(osl_relation_p relation, int row) {
	for (size_t j = 0; j < relation->nb_local_dims; j++) {
		if (!osl_int_zero(relation->precision,
						  relation->m[row][relation->nb_output_dims + relation->nb_input_dims + j])) {
			return 1;
		}
	}
	return 0;
}

/*
 * Remove all inequalities rows from an OSL relation.
 */
static void osl_relation_remove_inequalities(osl_relation_p relation) {
	for (int i = 0; i < relation->nb_rows; i++) {
		// if row is an inequality, remove it as the converter cannot handle
		// inequalities
		if (osl_int_one(relation->precision, relation->m[i][0])) {
			osl_relation_remove_row(relation, i);
			i--;
		}
	}
}

/*
 * Remove all inequalities from a list of OSL relations.
 */
static void osl_relation_list_remove_inequalities(osl_relation_list_p list) {
	for (osl_relation_list_p tmp = list; tmp; tmp = tmp->next) {
		osl_relation_remove_inequalities(tmp->elt);
	}
}

/*
 * Sanitizes the access relations in a list by removing inequality constraints.
 */
static void sanitize_stmt_access_relation(osl_relation_list_p list) {
	// remove inequalities from the access relations as the converter cannot
	// handle inequalities is this ok??
	osl_relation_list_remove_inequalities(list);
}

/* END  osl_pluto.c */

Scop2Isl::Scop2Isl(isl_ctx *ctx)
	: ctx(ctx)
	, scop(nullptr)
	, param_space(nullptr)
	, context(nullptr)
	, schedule(nullptr)
	, reads(nullptr)
	, writes(nullptr)
	, may_writes(nullptr) {}

Scop2Isl::~Scop2Isl() {
	if (this->domain != nullptr) {
		isl_union_set_free(this->domain);
	}
	if (this->context != nullptr) {
		isl_set_free(this->context);
	}
	if (this->param_space != nullptr) {
		isl_space_free(this->param_space);
	}
	if (this->schedule != nullptr) {
		isl_union_map_free(this->schedule);
	}
	if (this->reads != nullptr) {
		isl_union_map_free(this->reads);
	}
	if (this->writes != nullptr) {
		isl_union_map_free(this->writes);
	}
	if (this->may_writes != nullptr) {
		isl_union_map_free(this->may_writes);
	}
}

void Scop2Isl::convert(osl_scop_p scop) {
	this->scop = scop;

	// TODO: move to a separate function
	this->param_space = isl_space_params_alloc(ctx, this->scop->context->nb_parameters);
	if (this->scop->context->nb_parameters > 0) {
		osl_strings_p names = (osl_strings_p)this->scop->parameters->data;
		for (int i = 0; i < this->scop->context->nb_parameters; i++) {
			this->param_space = isl_space_set_dim_name(this->param_space, isl_dim_param, i, names->string[i]);
		}
	}

	this->context = osl_relation_to_isl_set(this->scop->context, isl_space_copy(this->param_space));

	this->domain = isl_union_set_empty_space(isl_space_copy(this->param_space));
	this->schedule = isl_union_map_empty_space(isl_space_copy(this->param_space));
	this->reads = isl_union_map_empty_space(isl_space_copy(this->param_space));
	this->writes = isl_union_map_empty_space(isl_space_copy(this->param_space));
	this->may_writes = isl_union_map_empty_space(isl_space_copy(this->param_space));

	osl_names_p names = get_scop_names(this->scop);

	unsigned i;
	osl_statement_p stmt;
	for (stmt = this->scop->statement, i = 0; stmt != NULL; stmt = stmt->next, ++i) {
		std::string stmt_name = "S_" + std::to_string(i);

		// parse domain
		isl_set *domain_i = this->islSetFromStatement(stmt);
		domain_i = isl_set_set_tuple_name(domain_i, stmt_name.c_str());
		domain_i = isl_set_intersect_params(domain_i, isl_set_copy(this->context));
		this->domain = isl_union_set_union(this->domain, isl_union_set_from_set(isl_set_copy(domain_i)));

		// parse scattering
		isl_map *schedule_i = this->islScheduleFromStatement(stmt);
		schedule_i = isl_map_set_tuple_name(schedule_i, isl_dim_in, stmt_name.c_str());
		this->schedule = isl_union_map_union(this->schedule, isl_union_map_from_map(isl_map_copy(schedule_i)));

		// parse access
		osl_relation_list_p sanitizedReadList = osl_relation_list_filter(stmt->access, OSL_TYPE_READ);
		osl_relation_list_p sanitizedMayWriteList = osl_relation_list_filter(stmt->access, OSL_TYPE_MAY_WRITE);
		osl_relation_list_p sanitizedWriteList = osl_relation_list_filter(stmt->access, OSL_TYPE_WRITE);

		sanitize_stmt_access_relation(sanitizedReadList);
		sanitize_stmt_access_relation(sanitizedMayWriteList);
		sanitize_stmt_access_relation(sanitizedWriteList);

		osl_arrays_p arrays = (osl_arrays_p)osl_generic_lookup(this->scop->extension, OSL_URI_ARRAYS);
		if (arrays) {
			if (names->arrays != nullptr) {
				osl_strings_free(names->arrays);
			}
			names->arrays = osl_arrays_to_strings(arrays);
		}

		isl_union_map *read_i =
			osl_access_list_to_isl_union_map(sanitizedReadList, isl_set_copy(domain_i), names->arrays->string);
		isl_union_map *write_i =
			osl_access_list_to_isl_union_map(sanitizedWriteList, isl_set_copy(domain_i), names->arrays->string);
		isl_union_map *mayWrite_i =
			osl_access_list_to_isl_union_map(sanitizedMayWriteList, isl_set_copy(domain_i), names->arrays->string);

		this->reads = isl_union_map_union(this->reads, read_i);
		this->writes = isl_union_map_union(this->writes, write_i);
		this->may_writes = isl_union_map_union(this->may_writes, mayWrite_i);

		osl_relation_list_free(sanitizedReadList);
		osl_relation_list_free(sanitizedMayWriteList);
		osl_relation_list_free(sanitizedWriteList);

		isl_set_free(domain_i);
		isl_map_free(schedule_i);
	}

	osl_names_free(names);
}

__isl_give isl_space *Scop2Isl::islDomainSpaceFromStatement(osl_statement_p stmt) {
	int niter = osl_statement_get_nb_iterators(stmt);
	int nparam = osl_scop_get_nb_parameters(this->scop);
	isl_space *space = isl_space_set_alloc(ctx, nparam, niter);
	if (nparam > 0) {
		osl_strings_p names = (osl_strings_p)this->scop->parameters->data;
		for (int i = 0; i < nparam; i++) {
			space = isl_space_set_dim_name(space, isl_dim_param, i, names->string[i]);
		}
	}
	if (niter > 0) {
		osl_body_p stmt_body = (osl_body_p)osl_generic_lookup(stmt->extension, OSL_URI_BODY);
		if (stmt_body != nullptr) {
			int nb_names = stmt_body ? osl_strings_size(stmt_body->iterators) : 0;
			for (int i = 0; i < niter && i < nb_names; i++) {
				space = isl_space_set_dim_name(space, isl_dim_set, i, stmt_body->iterators->string[i]);
			}
		}
	}
	return space;
}

__isl_give isl_set *Scop2Isl::islSetFromStatement(osl_statement_p stmt) {
	isl_space *space = this->islDomainSpaceFromStatement(stmt);

	return osl_relation_to_isl_set(stmt->domain, space);
}

__isl_give isl_map *Scop2Isl::islScheduleFromStatement(osl_statement_p stmt) {
	int niter = osl_statement_get_nb_iterators(stmt);
	int nparam = osl_scop_get_nb_parameters(this->scop);
	int nscat_dim = stmt->scattering->nb_output_dims;
	isl_space *space = isl_space_alloc(this->ctx, nparam, niter, nscat_dim);
	if (nparam > 0) {
		osl_strings_p names = (osl_strings_p)this->scop->parameters->data;
		for (int i = 0; i < nparam; i++) {
			space = isl_space_set_dim_name(space, isl_dim_param, i, names->string[i]);
		}
	}
	if (niter > 0) {
		osl_body_p stmt_body = (osl_body_p)osl_generic_lookup(stmt->extension, OSL_URI_BODY);
		if (stmt_body != nullptr) {
			for (int i = 0; i < niter; i++) {
				space = isl_space_set_dim_name(space, isl_dim_in, i, stmt_body->iterators->string[i]);
			}
		}
	}

	isl_map *schedule_i = osl_scattering_to_isl_map(stmt->scattering, space);

	return schedule_i;
}

/* Getters */
__isl_give isl_space *Scop2Isl::getParamSpace() const {
	return isl_space_copy(this->param_space);
}

__isl_give isl_set *Scop2Isl::getContext() const {
	return isl_set_copy(this->context);
}

__isl_give isl_union_set *Scop2Isl::getDomains() const {
	return isl_union_set_copy(this->domain);
}

__isl_give isl_union_map *Scop2Isl::getSchedules() const {
	return isl_union_map_copy(this->schedule);
}

__isl_give isl_union_map *Scop2Isl::getReads() const {
	return isl_union_map_copy(this->reads);
}

__isl_give isl_union_map *Scop2Isl::getWrites() const {
	return isl_union_map_copy(this->writes);
}

__isl_give isl_union_map *Scop2Isl::getMayWrites() const {
	return isl_union_map_copy(this->may_writes);
}
