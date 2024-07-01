#include "isl_support.h"
#include <isl/id.h>
#include <isl/map.h>
#include <isl/set.h>
#include <isl/space.h>
#include <isl/union_map.h>
#include <isl/union_set.h>
#include <osl/osl.h>

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

	int i;
	for (osl_statement_p stmt = scop->statement, i = 0; stmt != NULL;
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
