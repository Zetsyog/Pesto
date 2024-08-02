#ifndef PESTO_ISL_SUPPORT_H
#define PESTO_ISL_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <isl/constraint.h>
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

/**
 * Construct a basic set from two basic sets by taking the lowest and highest
 * constraints from each basic set.
 *
 * @param bset1
 * @param bset2
 * @return isl_basic_set *
 */
__isl_give isl_basic_set *
isl_basic_set_minmax_union(__isl_take isl_basic_set *bset1,
						   __isl_take isl_basic_set *bset2);

/**
 * Return the upper bound constraint for the given dimension in a basic set.
 * The constraint is returned if it exists, otherwise NULL is returned.
 *
 * @param bset
 * @param pos
 * @return isl_constraint *
 */
__isl_give isl_constraint *
isl_basic_set_dim_ub_cst(__isl_keep isl_basic_set *bset, int pos);

/**
 * Return the lower bound constraint for the given dimension in a basic set.
 * The constraint is returned if it exists, otherwise NULL is returned.
 *
 * @param bset
 * @param pos
 * @return isl_constraint *
 */
__isl_give isl_constraint *
isl_basic_set_dim_lb_cst(__isl_keep isl_basic_set *bset, int pos);

/**
 * Construct an isl_set from an osl_statement.
 * The scattering function will be applied to the domain
 * @param ctx
 * @param stmt
 * @return isl_set *
 */
__isl_give isl_set *isl_set_from_osl_stmt(isl_ctx *ctx, osl_statement_p stmt);

/**
 * Construct an isl_set from an osl_scop.
 * The scattering function will be applied to the domain of each statement
 * Each domain will be min-max unioned to create the final set
 * @param ctx
 * @param scop
 * @return isl_set *
 */
__isl_give isl_basic_set *isl_basic_set_from_osl_scop(isl_ctx *ctx,
													  osl_scop_p scop);

/**
 * Sets the param and dimensions name of the isl_set according to the osl_scop
 * @param set
 * @param scop
 * @return isl_set *
 */
__isl_give isl_set *
isl_set_set_param_names_from_osl_scop(__isl_take isl_set *set, osl_scop_p scop);

#ifdef __cplusplus
}
#endif

#endif // PESTO_ISL_SUPPORT_H