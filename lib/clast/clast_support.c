#include <assert.h>

#include "clast_support.h"

struct clast_stmt *clast_get_stmt_before(struct clast_stmt *root,
										 struct clast_stmt *stmt) {
	for (; root; root = root->next) {
		struct clast_stmt *syntax_next = clast_get_next(root);
		if (root->next == stmt || syntax_next == stmt) {
			return root;
		}
		if (clast_stmt_is_branch(stmt)) {
			struct clast_stmt *before =
				clast_get_stmt_before(syntax_next, stmt);
			if (before != NULL) {
				return before;
			}
		}
	}
	return NULL;
}

int clast_stmt_is_branch(struct clast_stmt *stmt) {
	return CLAST_STMT_IS_A(stmt, stmt_for) ||
		   CLAST_STMT_IS_A(stmt, stmt_guard) ||
		   CLAST_STMT_IS_A(stmt, stmt_block);
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

void clast_set_next(struct clast_stmt *stmt, struct clast_stmt *next) {
	*clast_get_next_ptr(stmt) = next;
}

struct clast_stmt *clast_insert_indepth(struct clast_stmt *before,
										struct clast_stmt *stmt) {
	stmt->next = clast_get_next(before);
	clast_set_next(before, stmt);
	return stmt;
}

struct clast_stmt *clast_get_next(struct clast_stmt *stmt) {
	return *clast_get_next_ptr(stmt);
}

struct clast_user_stmt *pesto_clast_create_stmt(CloogOptions *cloogOptions,
												CloogDomain *domain,
												unsigned int id) {
	CloogStatement *atiling_stmt =
		cloog_statement_alloc(cloogOptions->state, id);

	struct clast_user_stmt *user_stmt =
		new_clast_user_stmt(domain, atiling_stmt, NULL);
	return user_stmt;
}

struct clast_stmt *clast_insert_after(struct clast_stmt *before,
									  struct clast_stmt *stmt) {
	assert(before != NULL);
	if (stmt != NULL) {
		stmt->next = before->next;
	}
	before->next = stmt;
	return stmt;
}

struct clast_stmt *clast_insert_before(struct clast_stmt *root,
									   struct clast_stmt *before,
									   struct clast_stmt *stmt) {
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
			struct clast_stmt *inserted_stmt =
				clast_insert_before(deeper_stmt, before, stmt);
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