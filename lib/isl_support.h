#ifndef PESTO_ISL_SUPPORT_H
#define PESTO_ISL_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <isl/ctx.h>
#include <isl/map.h>
#include <isl/set.h>
#include <isl/union_map.h>
#include <osl/osl.h>

void osl_scop_to_isl(osl_scop_p scop, isl_ctx *ctx, isl_union_map **schedules,
					 isl_union_map **reads, isl_union_map **writes);

__isl_give isl_set *isl_set_from_osl_relation(osl_relation_p relation,
											  isl_ctx *ctx);
__isl_give isl_map *isl_map_from_osl_relation(osl_relation_p relation,
											  isl_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif // PESTO_ISL_SUPPORT_H