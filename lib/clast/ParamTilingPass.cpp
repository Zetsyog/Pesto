#include <functional>
#include <iostream>
#include <vector>

#include <pesto/Pesto.hpp>

#include <cloog/clast.h>

using namespace pesto;

ClastParamTilingPass::ClastParamTilingPass() {}

ClastParamTilingPass::~ClastParamTilingPass() {}

enum IteratorKind { IK_INNERTILE, IK_OUTERTILE };

class ClastIteratorInfo {
  public:
	struct clast_for *for_stmt;
	std::string name;
	IteratorKind kind;
	std::vector<std::string> depends_on;
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

std::vector<ClastIteratorInfo> retrieveIteratorInfo(struct clast_stmt *node) {
	std::vector<ClastIteratorInfo> iterators;
	traverseClastNode(
		[&](struct clast_stmt *node) {
			if (CLAST_STMT_IS_A(node, stmt_for)) {
				struct clast_for *for_stmt = (struct clast_for *)node;
				ClastIteratorInfo info;
				info.for_stmt = for_stmt;
				info.name = std::string(for_stmt->iterator);
				iterators.push_back(info);
			}
		},
		node);
	// find iterator types
	for (auto &info : iterators) {
		traverseClastNode(
			[&](struct clast_stmt *node) {
				if (CLAST_STMT_IS_A(node, stmt_for)) {
					struct clast_for *for_stmt = (struct clast_for *)node;
				}
			},
			info.for_stmt->body);
	}
	return iterators;
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
	log(std::cerr) << "Running pass on nest" << std::endl;
	std::vector<ClastIteratorInfo> itInfo =
		retrieveIteratorInfo(root->stmt.next);

	for (auto &info : itInfo) {
		std::cerr << "Iterator: " << info.name << std::endl;
	}
}

std::string ClastParamTilingPass::getUUID() { return "clast-param-tiling"; }