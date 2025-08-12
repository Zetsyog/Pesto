#ifndef PESTO_CLAST_SUPPORT_H
#define PESTO_CLAST_SUPPORT_H

#include <cloog/cloog.h>
#include <isl/set.h>
#include <osl/osl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Insert a statement after another following the depth first traversal of the
 * syntax tree
 * @return inserted statement
 */
struct clast_stmt *clast_insert_indepth(struct clast_stmt *before, struct clast_stmt *stmt);

/**
 * Insert a statement after another, they will be at the same level
 * @return inserted statement
 */
struct clast_stmt *clast_insert_after(struct clast_stmt *before, struct clast_stmt *stmt);

/**
 * Insert a statement before another, they will be at the same level
 * @return inserted statement
 */
struct clast_stmt *clast_insert_before(struct clast_stmt *root, struct clast_stmt *before, struct clast_stmt *stmt);

/**
 * Return a pointer to the next statement deeper in the syntax tree
 * according to the depth first traversal
 * @param stmt the branch statement
 * @return NULL if there is no deeper statement
 */
struct clast_stmt *clast_branch_deeper_next(struct clast_stmt *stmt);

/*
  Return the next statement according to the depth first traversal of the
	syntax tree
*/
struct clast_stmt *clast_get_next(struct clast_stmt *stmt);

int clast_stmt_is_branch(struct clast_stmt *stmt);

struct clast_stmt *clast_get_stmt_before(struct clast_stmt *root, struct clast_stmt *stmt);

/**
 * Return a pointer to next statement according to the depth first traversal of
 * the syntax tree
 */
struct clast_stmt **clast_get_next_ptr(struct clast_stmt *stmt);

struct clast_user_stmt *pesto_clast_create_stmt(CloogOptions *cloogOptions, CloogDomain *domain, unsigned int id);

int clast_for_is_outter_tile_loop(__isl_keep isl_set *domain, struct clast_for *for_stmt, int tilesize);

int clast_for_get_outter_tile_depth(__isl_keep isl_set *domain, struct clast_stmt *root, struct clast_for *outter_for,
									int tilesize);

int clast_for_is_inner_tile_loop(struct clast_for *for_stmt, int tile_size);

/**
 * For a given clast_for, return the index of the isl_dimension
 * that reprensent the corresponding outter tiling dimension
 * @param for_stmt
 * @param tile_size the size of the done tiling
 * @return index of the outter tiling dimension or -1 if the loop is not tiled
 */
int clast_for_get_inner_tile_outter_dim(struct clast_for *for_stmt, int tile_size);

/**
 * For a given clast_for, return the corresponding outter tiling clast_for
 * @param root the root of the syntax tree
 * @param inner_for the inner tiling loop
 * @param tile_size hint on the tile size used for tiling
 * @return the outter tiling loop or NULL if the loop is not tiled
 */
struct clast_for *clast_get_outter_tiling_for(struct clast_stmt *root, struct clast_for *inner_for, int tile_size);

/**
 * For a given clast, traverse the syntax tree and return all the user
 * statements
 * @param root the root of the syntax tree
 * @param size the number of user statements
 */
struct clast_user_stmt **clast_get_user_stmt_list(struct clast_stmt *root, int *size);

/**
 * Return the parent loop of a given statement
 * @param root the root of the syntax tree
 * @param stmt the statement
 * @return the parent loop or NULL if the statement is not in a loop
 */
struct clast_for *clast_get_parent_clast_for(struct clast_stmt *root, struct clast_stmt *stmt);

/**
 * Compute the depth of a given clast_for inside the syntax tree
 * Depth is the number of for loops that are the parent of the given for loop
 * @param root the root of the syntax tree
 * @param for_stmt the for statement
 */
int clast_for_depth(struct clast_stmt *root, struct clast_for *for_stmt);

/**
 * Compute the original bound (upper or lower) of a given clast_for
 * By original, it means the bound before any tiling transformation
 * @param for_stmt the for statement
 * @param bound the bound to get (0 for lower, 1 for upper)
 * @return the computed bound
 */
struct clast_expr *clast_for_compute_original_bound(CloogNames *names, struct clast_for *inner_for,
													struct clast_for *outter_for, int bound);

/**
 * Create a hard copy of a CloogNames structure
 */
CloogNames *cloog_names_hard_copy(CloogNames *names);

/**
 * add tags clast loops according to information in scop's osl_loop extension
 * Stolen from cloog source
 */
int clast_annotate_loops(osl_scop_p program, struct clast_stmt *root);

/**
 * TODO: document
 */
struct clast_expr *clast_bound_from_isl_set(CloogNames *names, __isl_keep isl_set *set, int bound, int dim);

/**
 * @brief search for the corresponding outter tiling for of the given clast for
 * This function will search among all the clast for that contains @param inner_for
 * and return the one that iterate over the tile space of @param inner_for
 * @param root the root of the clast tree
 * @param inner_for the inner for to search for
 * @return struct clast_for* the outter for that contains the inner for
 * @note the function will return NULL if no outter for is found
 */
struct clast_for *clast_find_outter_tiling_for(struct clast_stmt *root, struct clast_for *inner_for);

/**
 * @brief insert a clast_for that encloses the given statement
 * @param root the root of the clast tree
 * @param stmt the statement to enclose
 * @return the inserted clast_for
 */
struct clast_for *clast_insert_enclosing_for(struct clast_stmt *root, struct clast_stmt *stmt, CloogDomain *domain);

/**
 * @brief return the innermost clast_for in a given block
 * @param root_for the outermost for loop
 * @return the innermost clast_for in the given block
 */
struct clast_for *clast_get_innermost_for(struct clast_stmt *root_for);

struct clast_stmt *clast_stmt_hard_copy(struct clast_stmt *stmt);

#ifdef __cplusplus
}
#endif

#endif // PESTO_CLAST_SUPPORT_H