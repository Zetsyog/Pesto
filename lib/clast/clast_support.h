#ifndef PESTO_CLAST_SUPPORT_H
#define PESTO_CLAST_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cloog/cloog.h>

/**
 * Insert a statement after another following the depth first traversal of the
 * syntax tree
 * @return inserted statement
 */
struct clast_stmt *clast_insert_indepth(struct clast_stmt *before,
										struct clast_stmt *stmt);

/**
 * Insert a statement after another, they will be at the same level
 * @return inserted statement
 */
struct clast_stmt *clast_insert_after(struct clast_stmt *before,
									  struct clast_stmt *stmt);

/**
 * Insert a statement before another, they will be at the same level
 * @return inserted statement
 */
struct clast_stmt *clast_insert_before(struct clast_stmt *root,
									   struct clast_stmt *before,
									   struct clast_stmt *stmt);

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

struct clast_stmt *clast_get_stmt_before(struct clast_stmt *root,
										 struct clast_stmt *stmt);

/**
 * Return a pointer to next statement according to the depth first traversal of
 * the syntax tree
 */
struct clast_stmt **clast_get_next_ptr(struct clast_stmt *stmt);

struct clast_user_stmt *pesto_clast_create_stmt(CloogOptions *cloogOptions,
												CloogDomain *domain,
												unsigned int id);

#ifdef __cplusplus
}
#endif

#endif // PESTO_CLAST_SUPPORT_H