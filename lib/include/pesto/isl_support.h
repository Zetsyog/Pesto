/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <isl/constraint.h>
#include <isl/ctx.h>
#include <isl/map.h>
#include <isl/mat.h>
#include <isl/set.h>
#include <isl/space.h>
#include <isl/space_type.h>
#include <isl/union_map.h>
#include <isl/vec.h>

#include <osl/osl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Construct a basic set from two basic sets by taking the lowest and highest
 * constraints from each basic set.
 *
 * @param bset1
 * @param bset2
 * @return isl_basic_set *
 */
__isl_give isl_basic_set *isl_basic_set_minmax_union(__isl_take isl_basic_set *bset1, __isl_take isl_basic_set *bset2);

/**
 * Return the upper bound constraint for the given dimension in a basic set.
 * The constraint is returned if it exists, otherwise NULL is returned.
 *
 * @param bset
 * @param pos
 * @return isl_constraint *
 */
__isl_give isl_constraint *isl_basic_set_dim_ub_cst(__isl_keep isl_basic_set *bset, int pos);

/**
 * Return the lower bound constraint for the given dimension in a basic set.
 * The constraint is returned if it exists, otherwise NULL is returned.
 *
 * @param bset
 * @param pos
 * @return isl_constraint *
 */
__isl_give isl_constraint *isl_basic_set_dim_lb_cst(__isl_keep isl_basic_set *bset, int pos);

/**
 * Sets the param and dimensions name of the isl_set according to the osl_scop
 * @param set
 * @param scop
 * @return isl_set *
 */
__isl_give isl_set *isl_set_set_param_names_from_osl_scop(__isl_take isl_set *set, osl_scop_p scop);

/**
 * Check if the given dimension in the basic set is used as a tiling dimension
 * @param bset
 * @param pos
 * @return isl_bool
 */
isl_bool isl_basic_set_is_tiling_dim(__isl_keep isl_basic_set *bset, int pos);

/**
 * @brief get the number of tiling dimensions of the relation
 * @param set
 * @return int
 */
int isl_set_count_tiling_dimensions(__isl_keep isl_set *set);

/**
 * Removes any dimension that is a tiling iterator from the space of the set
 * @param set
 * @return a new set with the tiling dimensions removed
 */
__isl_give isl_set *isl_set_remove_tiling_dimensions(__isl_take isl_set *set);

/**
 * Drop all constraints involving a tiling dimension from the set
 * @param set
 * @return a new set with the tiling constraints removed
 */
__isl_give isl_set *isl_set_drop_tiling_constraints(__isl_take isl_set *set);

/**
 *  @brief check if the given dimension is scalar in the set
 * @param set
 * @param dim
 * @return isl_bool whether the dimension is scalar
 */
isl_bool isl_set_is_dim_scalar(__isl_keep isl_set *set, int dim);

__isl_give isl_vec *isl_mat_row_at(__isl_keep isl_mat *mat, int row);

/**
 * @brief check if the given dimension is a tiling dimension in the set
 * @param set
 * @param pos
 * @return isl_bool
 */
isl_bool isl_set_is_tiling_dim(__isl_keep isl_set *set, int pos);

/**
 * @brief compute a map from the set dimension to a unique dimension given by @param dim.
 * This map can be used to compute the bounds of the given dimension in the set
 * @param set
 * @param dim
 * @return isl_map *
 */
__isl_give isl_map *isl_set_compute_bound_map(__isl_take isl_set *set, int dim);

/**
 * @brief get the lower bound of the given dimension in the set
 * @param set
 * @param dim
 * @return isl_pw_aff *
 */
__isl_give isl_pw_aff *isl_set_dim_lower_bound(__isl_take isl_set *set, int dim);

/**
 * @brief get the upper bound of the given dimension in the set
 * @param set
 * @param dim
 * @return isl_pw_aff *
 */
__isl_give isl_pw_aff *isl_set_dim_upper_bound(__isl_take isl_set *set, int dim);

#ifdef __cplusplus
}
#endif
