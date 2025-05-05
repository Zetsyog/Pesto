#include <pesto/clast/CustomVecPragma.hpp>
#include <pesto/clast/GenClastPass.hpp>
#include <pesto/clast/clast_support.h>

#include <functional>

using pesto::pass::CustomVecPragma;

CustomVecPragma::CustomVecPragma() : pesto::pass::IPass("clast-vec-pragma"), pragma("#pragma ivdep") {}

CustomVecPragma::~CustomVecPragma() {}

void FindVecLoopRecursive(std::function<void(clast_for *)> callback, clast_stmt *stmt) {
	if (stmt == nullptr) {
		return;
	}

	if (CLAST_STMT_IS_A(stmt, stmt_for)) {
		auto for_stmt = (clast_for *)stmt;
		if (for_stmt->parallel & CLAST_PARALLEL_VEC) {
			callback(for_stmt);
		}
	}

	if (clast_stmt_is_branch(stmt)) {
		FindVecLoopRecursive(callback, clast_branch_deeper_next(stmt));
	}

	FindVecLoopRecursive(callback, stmt->next);
}

void CustomVecPragma::applyScop(OslScop &nest) {
	using metadata::ClastHolder;

	auto data = nest.metadata.get<ClastHolder>().lock();

	clast_stmt *root = data->root;

	// we want to do a depth first traversal of the clast
	FindVecLoopRecursive(
		[this](clast_for *for_stmt) {
			for_stmt->parallel |= CLAST_PARALLEL_USER;
			if (for_stmt->user_directive != nullptr) {
				free(for_stmt->user_directive);
			}
			for_stmt->user_directive = strdup(pragma.c_str());
		},
		root);
}

void CustomVecPragma::setPragma(std::string pragma) { this->pragma = pragma; }
