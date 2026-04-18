/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/clast_support.h>
#include <pesto/ir/ClastIR.hpp>
#include <pesto/pass/clast/CustomVecPragma.hpp>
#include <pesto/pass/clast/GenClastPass.hpp>

#include <functional>

using pesto::Bundle;
using pesto::ir::ClastIR;
using pesto::pass::CustomVecPragma;

CustomVecPragma::CustomVecPragma() : pesto::pass::IPass("clast-vec-pragma"), pragma("#pragma ivdep") {}

CustomVecPragma::~CustomVecPragma() {}

std::set<std::string> CustomVecPragma::inputIR() const {
	return {pesto::ir::ClastIR::ID};
}

std::string CustomVecPragma::outputIR() const {
	return pesto::ir::ClastIR::ID;
}

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

Bundle CustomVecPragma::apply(Bundle &&bundle) {
	ClastIR *clastIR = dynamic_cast<ClastIR *>(bundle.get_ir());

	clast_stmt *root = clastIR->get();

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
	return std::move(bundle);
}

void CustomVecPragma::setPragma(std::string pragma) {
	this->pragma = pragma;
}
