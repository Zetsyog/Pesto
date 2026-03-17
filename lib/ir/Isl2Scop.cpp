/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <stdlib.h>
#include <string.h>

#include <iostream>

#include <isl/aff.h>
#include <isl/map.h>
#include <isl/mat.h>
#include <isl/schedule_node.h>
#include <isl/set.h>
#include <isl/space.h>
#include <isl/union_map.h>
#include <isl/union_set.h>
#include <isl/val.h>
#include <isl/val_gmp.h>

#include <osl/osl.h>

#include <pesto/ir/Isl2Scop.hpp>

using pesto::ir::converter::Isl2Scop;

Isl2Scop::Isl2Scop() {}

Isl2Scop::~Isl2Scop() {}

static osl_relation_p isl_basic_map_to_osl_relation(int precision, __isl_take isl_basic_map *bmap) {
	isl_space *space = isl_basic_map_get_space(bmap);
	isl_size nparam = isl_space_dim(space, isl_dim_param);
	isl_size ndimin = isl_space_dim(space, isl_dim_in);
	isl_size ndimout = isl_space_dim(space, isl_dim_out);

	osl_relation_p relation = osl_relation_pmalloc(precision, 0, nparam + ndimin + ndimout + 2);
	relation->type = OSL_TYPE_SCATTERING;
	relation->nb_parameters = nparam;
	relation->nb_output_dims = ndimin;
	relation->nb_input_dims = ndimout;
	relation->nb_local_dims = 0;

	isl_basic_map_dump(bmap);

	isl_space_dump(space);
	space = isl_space_drop_dims(space, isl_dim_in, 0, isl_space_dim(space, isl_dim_in));
	space = isl_space_move_dims(space, isl_dim_in, 0, isl_dim_out, 0, isl_space_dim(space, isl_dim_out));
	space = isl_space_add_dims(space, isl_dim_out, isl_space_dim(space, isl_dim_in));
	isl_space_dump(space);
	isl_map *id = isl_map_identity(space);
	isl_map_dump(id);
	for (isl_size i = 0; i < ndimout; ++i) {
	}

	osl_relation_dump(stderr, relation);

	return relation;
}

static osl_relation_p isl_map_to_osl_scat(int precision, isl_map *map) {
	isl_ctx *ctx = isl_map_get_ctx(map);

	isl_printer *p = isl_printer_to_str(ctx);
	p = isl_printer_set_output_format(p, ISL_FORMAT_EXT_POLYLIB);
	p = isl_printer_print_str(p, "SCATTERING");
	p = isl_printer_end_line(p);
	p = isl_printer_print_map(p, map);
	char *str = isl_printer_get_str(p);
	char *endptr = str;
	osl_relation_p relation = osl_relation_psread(&endptr, precision);
	free(str);

	isl_printer_free(p);

	// isl_size orig_nin = isl_map_dim(map, isl_dim_in);

	// isl_map *schedule_range = isl_map_range_map(map);
	// schedule_range = isl_map_flatten(schedule_range);
	// schedule_range = isl_map_remove_dims(schedule_range, isl_dim_in, 0, orig_nin);

	// isl_basic_map_list *bmap_list = isl_map_get_basic_map_list(schedule_range);
	// isl_basic_map *bmap = isl_basic_map_list_get_at(bmap_list, 0);

	// isl_mat *eq =
	// 	isl_basic_map_equalities_matrix(bmap, isl_dim_out, isl_dim_in, isl_dim_div, isl_dim_param, isl_dim_cst);

	// isl_size nrows = isl_mat_rows(eq);
	// isl_size ncols = isl_mat_cols(eq);
	// isl_size nparam = isl_map_dim(schedule_range, isl_dim_param);
	// isl_size nout = isl_map_dim(schedule_range, isl_dim_out);
	// isl_size nin = isl_map_dim(schedule_range, isl_dim_in);
	// isl_size nlocal = isl_map_dim(schedule_range, isl_dim_div);

	// relation = osl_relation_pmalloc(precision, nrows, nout + nin + nlocal + nparam + 2);
	// relation->type = OSL_TYPE_SCATTERING;
	// relation->nb_parameters = nparam;
	// relation->nb_output_dims = nout;
	// relation->nb_input_dims = nin;
	// relation->nb_local_dims = nlocal;
	// relation->nb_rows = nrows;
	// relation->nb_columns = nout + nin + nlocal + nparam + 2;

	// for (int i = 0; i < nrows; ++i) {
	// 	osl_int_set_si(relation->precision, &relation->m[i][0], 0);
	// 	for (int j = 0; j < ncols; ++j) {
	// 		int t = isl_val_get_num_si(isl_mat_get_element_val(eq, i, j));
	// 		osl_int_set_si(relation->precision, &relation->m[i][1 + j], t);
	// 	}
	// }

	// isl_mat_free(eq);
	// isl_basic_map_free(bmap);
	// isl_basic_map_list_free(bmap_list);

	return relation;
}

void Isl2Scop::applySchedule(osl_scop_p scop, isl_schedule *schedule) {
	isl_ctx *ctx = isl_schedule_get_ctx(schedule);

	isl_union_set *domain = isl_schedule_get_domain(schedule);
	isl_union_map *schedule_map = isl_schedule_get_map(schedule);

	isl_map_list *map_list = isl_union_map_get_map_list(schedule_map);
	isl_size nmap = isl_map_list_size(map_list);
	isl_set_list *domain_list = isl_union_set_get_set_list(domain);
	isl_size ndomain = isl_set_list_size(domain_list);

	std::cerr << "nmap: " << nmap << std::endl;
	std::cerr << "ndomain: " << ndomain << std::endl;

	isl_printer *p = isl_printer_to_file(ctx, stderr);
	p = isl_printer_set_output_format(p, ISL_FORMAT_EXT_POLYLIB);

	osl_statement_p stmt = nullptr;
	int i = 0;
	for (stmt = scop->statement, i = 0; stmt != nullptr; stmt = stmt->next, ++i) {
		isl_map *map = nullptr;
		for (isl_size j = 0; j < nmap; ++j) {
			isl_map *tmp = isl_map_list_get_at(map_list, j);
			const char *name = isl_map_get_tuple_name(tmp, isl_dim_in);
			int id = atoi(name + 2);
			if (id == i) {
				map = tmp;
				break;
			}
		}

		osl_relation_p scat = isl_map_to_osl_scat(stmt->scattering->precision, map);
		osl_relation_free(stmt->scattering);
		stmt->scattering = scat;

		isl_map_free(map);
	}

	if (osl_generic_has_URI(scop->extension, OSL_URI_SCATNAMES)) {
		osl_scatnames_p scatnames = (osl_scatnames_p)osl_generic_lookup(scop->extension, OSL_URI_SCATNAMES);
		int size = osl_strings_size(scatnames->names);
		osl_generic_remove(&scop->extension, OSL_URI_SCATNAMES);
		scatnames = osl_scatnames_malloc();
		scatnames->names = osl_strings_generate("t", size);
		osl_generic_add(&scop->extension, osl_generic_shell(scatnames, osl_scatnames_interface()));
	}

	isl_printer_free(p);
}