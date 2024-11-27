#include <pesto/clast/CustomVecPragma.hpp>
#include <pesto/clast/GenClastPass.hpp>
#include <pesto/clast/clast_support.h>

using pesto::pass::CustomVecPragma;

CustomVecPragma::CustomVecPragma()
	: pesto::pass::IPass("clast-vec-pragma"), pragma("#pragma ivdep") {}

CustomVecPragma::~CustomVecPragma() {}

void CustomVecPragma::applyScop(OslScop &nest) {
	using metadata::ClastHolder;

	auto data = nest.metadata.get<ClastHolder>().lock();

	clast_stmt *root = data->root;

	// we want to do a depth first traversal of the clast
	clast_stmt *stmt = root;
	for (; stmt != NULL; stmt = clast_get_next(stmt)) {
		if (CLAST_STMT_IS_A(stmt, stmt_for)) {
			clast_for *for_stmt = (clast_for *)stmt;
			if (for_stmt->parallel & CLAST_PARALLEL_VEC) {
				log(std::cerr)
					<< "Found a vectorized for loop (nest_" << nest.getId()
					<< "; " << for_stmt->iterator << ")" << std::endl;
				if (for_stmt->user_directive != nullptr) {
					free(for_stmt->user_directive);
				}

				for_stmt->parallel |= CLAST_PARALLEL_USER;
				for_stmt->user_directive = strdup(pragma.c_str());
			}
		}
	}
}

void CustomVecPragma::setPragma(std::string pragma) { this->pragma = pragma; }
