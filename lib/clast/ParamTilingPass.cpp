#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <cloog/cloog.h>

#include <pesto/Pesto.hpp>

#include "clast/clast_support.h"
#include "pesto/trahrhe.h"

using namespace pesto;

ClastParamTilingPass::ClastParamTilingPass() {}

ClastParamTilingPass::~ClastParamTilingPass() {}

enum IteratorType { IT_DEFAULT, IT_INNERTILE, IT_OUTERTILE };

/**
 * ClastIteratorInfo
 * A class to store information about an iterator in a clast tree
 * It will store dependencies between tiling and tiled iterators
 */
class ClastIteratorInfo {
  public:
	struct clast_for *for_stmt;
	std::string name;
	IteratorType type;
	std::vector<struct clast_for *> depends_on;

	void dump(std::ostream &os) {
		os << "Iterator: " << name << std::endl;
		os << "\tKind: ";
		switch (type) {
		case IT_DEFAULT:
			os << "Default";
			break;
		case IT_INNERTILE:
			os << "Inner Tile";
			break;
		case IT_OUTERTILE:
			os << "Outer Tile";
			break;
		}
		os << std::endl;
		if (depends_on.size() > 0) {
			os << "\tDepends on: ";
			for (auto &dep : depends_on) {
				os << dep->iterator << " ";
			}
			os << std::endl;
		}
	}
};

void traverseClastNode(std::function<void(struct clast_stmt *)> cb,
					   struct clast_stmt *node) {
	if (node == nullptr) {
		return;
	}
	cb(node);
	if (CLAST_STMT_IS_A(node, stmt_for)) {
		struct clast_for *for_stmt = (struct clast_for *)node;
		struct clast_stmt *body = for_stmt->body;
		while (body != nullptr) {
			traverseClastNode(cb, body);
			body = body->next;
		}
	}
	if (CLAST_STMT_IS_A(node, stmt_guard)) {
		struct clast_guard *guard_stmt = (struct clast_guard *)node;
		struct clast_stmt *body = guard_stmt->then;
		while (body != nullptr) {
			traverseClastNode(cb, body);
			body = body->next;
		}
	}
}

void traverseClastExpr(std::function<int(struct clast_expr *)> cb,
					   struct clast_expr *expr) {
	if (expr == nullptr) {
		return;
	}
	int quit = cb(expr);
	if (quit) {
		return;
	}
	if (expr->type == clast_expr_name) {
		return;
	}
	if (expr->type == clast_expr_term) {
		struct clast_term *term = (struct clast_term *)expr;
		if (term->var != nullptr) {
			traverseClastExpr(cb, term->var);
		}
	}
	if (expr->type == clast_expr_bin) {
		struct clast_binary *bin = (struct clast_binary *)expr;
		if (bin->LHS != nullptr) {
			traverseClastExpr(cb, bin->LHS);
		}
	}
	if (expr->type == clast_expr_red) {
		struct clast_reduction *red = (struct clast_reduction *)expr;
		for (int i = 0; i < red->n; i++) {
			traverseClastExpr(cb, red->elts[i]);
		}
	}
}

void setIteratorKind(std::vector<std::unique_ptr<ClastIteratorInfo>> &iterators,
					 struct clast_for *for_stmt, IteratorType type) {
	for (auto &info : iterators) {
		if (info->for_stmt == for_stmt) {
			info->type = type;
		}
	}
}

void addIteratorDependency(
	std::vector<std::unique_ptr<ClastIteratorInfo>> &iterators,
	struct clast_for *for_stmt, struct clast_for *depends_on) {
	for (auto &info : iterators) {
		if (info->for_stmt == for_stmt) {
			info->depends_on.push_back(depends_on);
		}
	}
}

std::vector<std::unique_ptr<ClastIteratorInfo>>
retrieveIteratorInfo(struct clast_stmt *node) {
	std::vector<std::unique_ptr<ClastIteratorInfo>> iterators;
	traverseClastNode(
		[&](struct clast_stmt *node) {
			if (CLAST_STMT_IS_A(node, stmt_for)) {
				struct clast_for *for_stmt = (struct clast_for *)node;
				auto info = std::make_unique<ClastIteratorInfo>();
				info->for_stmt = for_stmt;
				info->name = std::string(for_stmt->iterator);
				info->type = IT_DEFAULT;
				iterators.push_back(std::move(info));
			}
		},
		node);
	// find iterator types
	for (auto &info : iterators) {
		traverseClastNode(
			[&](struct clast_stmt *node) {
				if (CLAST_STMT_IS_A(node, stmt_for)) {
					struct clast_for *for_stmt = (struct clast_for *)node;
					traverseClastExpr(
						[&](struct clast_expr *expr) {
							if (expr->type == clast_expr_term) {
								struct clast_term *term =
									(struct clast_term *)expr;
								if (cloog_int_ne_si(term->val, 32)) {
									return 0;
								}
								if (term->var != nullptr &&
									term->var->type == clast_expr_name) {
									struct clast_name *name =
										(struct clast_name *)term->var;
									if (std::string(name->name) == info->name) {
										info->type = IT_OUTERTILE;
										setIteratorKind(iterators, for_stmt,
														IT_INNERTILE);
										addIteratorDependency(iterators,
															  for_stmt,
															  info->for_stmt);
										return 1;
									}
								}
							}
							return 0;
						},
						for_stmt->LB);
				}
			},
			(struct clast_stmt *)info->for_stmt);
	}
	return iterators;
}

/**
 * Insert statements where tiling bounds will be computed
 * TODO: handle wavefront paralellism
 */
void insert_tiling_statements(
	ClastMetadata &data, struct clast_stmt *root,
	std::vector<std::unique_ptr<ClastIteratorInfo>> &itInfo) {
	// for each tiling iterator, we should insert a stmt before (header) and
	// inside (footer)
	for (auto &it : itInfo) {
		if (it->type == IT_OUTERTILE) {
			struct clast_for *for_loop = it->for_stmt;
			struct trahrhe_codegen_data gen_data[2];

			struct clast_user_stmt *header_stmt = pesto_clast_create_stmt(
				data.cloogOptions, for_loop->domain, ++data.stmtsNb);
			clast_insert_before(root, (struct clast_stmt *)for_loop,
								(struct clast_stmt *)header_stmt);
			gen_data[0].stmt_type = head;
			gen_data[0].tiling_type = algebraic;
			gen_data[0].stmt_id = data.stmtsNb;
			gen_data[0].iterator_name = strdup(for_loop->iterator);
			// TODO
			gen_data[0].band_id = 0;
			gen_data[0].depth = 0;
			gen_data[0].lb_expr = strdup("lb");
			gen_data[0].ub_expr = strdup("ub");
			gen_data[0].npar = 0;
			gen_data[0].params = NULL;
			gen_data[0].endline = strdup("\\\n");

			struct clast_user_stmt *footer_stmt = pesto_clast_create_stmt(
				data.cloogOptions, for_loop->domain, ++data.stmtsNb);
			clast_insert_indepth((struct clast_stmt *)for_loop,
								 (struct clast_stmt *)footer_stmt);

			trahrhe_codegen_stmt(stderr, &gen_data[0]);

			for (int i = 0; i < 1; i += 1) {
				free(gen_data[i].iterator_name);
				free(gen_data[i].lb_expr);
				free(gen_data[i].ub_expr);
				free(gen_data[i].endline);
			}
		}
	}
}

void ClastParamTilingPass::run(PolyProgram &prog) {
	// ensure each nest has a clast metadata
	for (auto &nest : prog.getScopNests()) {
		if (!nest->metadata.has("clast")) {
			log(std::cerr)
				<< "Nest does not have clast metadata. You should "
				   "consider running 'clast-gen' pass before this one."
				<< std::endl;
			log(std::cerr) << "Aborting pass." << std::endl;
			return;
		}
	}

	// run the pass on each nest
	for (auto &nest : prog.getScopNests()) {
		runSingleNest(*nest);
	}
}

void ClastParamTilingPass::runSingleNest(ScopNest &nest) {
	ClastMetadata metadata = nest.metadata.get<ClastMetadata>("clast");
	struct clast_root *root = (struct clast_root *)metadata.root;
	if (root == NULL) {
		return;
	}
	log(std::cerr) << "Running pass on nest " << nest.getId() << std::endl;

	// retrieve data about iterators
	auto itInfo = retrieveIteratorInfo(root->stmt.next);

	// dump iterator info
	// DEBUG
	// TODO: remove this
	for (auto &info : itInfo) {
		info->dump(std::cerr);
	}

	insert_tiling_statements(metadata, metadata.root, itInfo);
}

std::string ClastParamTilingPass::getUUID() { return "clast-param-tiling"; }