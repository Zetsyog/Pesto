#include <vector>

#include <isl/aff.h>
#include <isl/ast_build.h>
#include <isl/flow.h>
#include <isl/maybe_pw_aff.h>
#include <isl/multi.h>
#include <isl/printer.h>
#include <isl/schedule.h>
#include <isl/schedule_node.h>
#include <isl/union_set.h>
#include <isl/val.h>

#include <cloog/cloog.h>

#include <pesto/CodegenPasses.hpp>
#include <pesto/OslPass.hpp>
#include <pesto/Pluto.hpp>
#include <pesto/PlutoPass.hpp>
#include <pesto/clast/clast_support.h>
#include <pesto/ir/Isl2Scop.hpp>
#include <pesto/ir/Scop2Isl.hpp>
#include <pesto/macros.h>
#include <pesto/pass/SplitTilingPass.hpp>

// TODO: rename this pass to TrapezoidalTilingPass

using pesto::pass::SplitTilingPass;

SplitTilingPass::SplitTilingPass() : IPass("split-tiling") { this->ctx = isl_ctx_alloc(); }

SplitTilingPass::~SplitTilingPass() { isl_ctx_free(this->ctx); }

namespace pesto::pass {
class ClastTilingPair {
  public:
	struct clast_for *innerFor;
	struct clast_for *outerFor;

	struct clast_user_stmt *outerHeaderStmt;
	struct clast_user_stmt *outerBodyStmt;
};
} // namespace pesto::pass

void SplitTilingPass::applyScop(OslScop &scop) {
	GenClastPass genClastPass;
	genClastPass.applyScop(scop);

	if (!scop.metadata.has<metadata::ClastHolder>()) {
		std::cerr << "[SplitTilingPass] Clast generation failed. Could not apply Trapezoidal Tiling" << std::endl;
		return;
	}

	auto clastData = scop.metadata.get<metadata::ClastHolder>().lock();

	auto root = (struct clast_root *)clastData->root;
	auto cloogProgram = clastData->cloogProgram;
	auto cloogOptions = clastData->cloogOptions;

	auto stmts = clast_get_user_stmt_list(&(root->stmt), &this->insertedStmts);
	free(stmts);

	for (struct clast_stmt *stmt = &(root->stmt); stmt != NULL; stmt = clast_get_next(stmt)) {
		if (CLAST_STMT_IS_A(stmt, stmt_for)) {
			struct clast_for *clastFor = (struct clast_for *)stmt;

			tileClastFor(scop, cloogProgram, cloogOptions, root, clastFor);
			break;
		}
	}
}

void SplitTilingPass::tileClastFor(OslScop &scop, CloogProgram *cloogProgram, CloogOptions *cloogOptions,
								   struct clast_root *root, struct clast_for *clastFor) {
	using pesto::pass ::ClastTilingPair;
	using pesto::pass::metadata::MacroMetadata;
	using pesto::pass::metadata::VarDeclMetadata;

	auto macroData = scop.metadata.getOrCreate<MacroMetadata>().lock();
	auto varDeclData = scop.metadata.getOrCreate<VarDeclMetadata>().lock();

	log(std::cerr) << "tiling clastFor: " << clastFor->iterator << std::endl;
	CloogDomain *domain = cloog_domain_universe(cloogOptions->state, cloog_domain_dimension(clastFor->domain));

	std::vector<ClastTilingPair> clastTilingPairs;

	for (struct clast_for *for_it = clast_get_innermost_for(&(clastFor->stmt)); for_it != NULL;
		 for_it = clast_get_parent_clast_for(&(root->stmt), &(for_it->stmt))) {
		ClastTilingPair pair;
		pair.innerFor = for_it;
		log(std::cerr) << "Found inner for: " << for_it->iterator << std::endl;

		clastTilingPairs.push_back(pair);
	}
	for (auto it = clastTilingPairs.rbegin(); it != clastTilingPairs.rend(); ++it) {
		// Access *it as a ClastTilingPair
		// Example: log(std::cerr) << "Processing pair with innerFor: " << it->innerFor->iterator << std::endl;
		struct clast_for *enclosing_for = clast_insert_enclosing_for(&(root->stmt), &(clastFor->stmt), domain);

		// Set iterator name
		std::unique_ptr<std::string> it_name = std::make_unique<std::string>("z" + std::string(it->innerFor->iterator));
		std::unique_ptr<std::string> lbName = std::make_unique<std::string>("lb" + *it_name);
		std::unique_ptr<std::string> ubName = std::make_unique<std::string>("ub" + *it_name);

		varDeclData->addVar(*it_name);
		varDeclData->addVar(*lbName);
		varDeclData->addVar(*ubName);

		enclosing_for->iterator = it_name->c_str(); // Set the iterator name for the enclosing loop

		enclosing_for->LB = &(new_clast_name(lbName->c_str())->expr);
		enclosing_for->UB = &(new_clast_name(ubName->c_str())->expr);

		it->outerFor = enclosing_for;

		this->stringKeepAlive.push_back(std::move(it_name));
		this->stringKeepAlive.push_back(std::move(lbName));
		this->stringKeepAlive.push_back(std::move(ubName));

		it->innerFor->parallel = CLAST_PARALLEL_NOT; // Disable parallelism for the inner loop

		this->insertedStmts += 1;
		it->outerHeaderStmt = pesto_clast_create_stmt(cloogOptions, nullptr, this->insertedStmts);
		clast_insert_before(&(root->stmt), &(it->outerFor->stmt), &(it->outerHeaderStmt->stmt));

		this->insertedStmts += 1;
		it->outerBodyStmt = pesto_clast_create_stmt(cloogOptions, nullptr, this->insertedStmts);
		clast_insert_indepth(&(it->outerFor->stmt), &(it->outerBodyStmt->stmt));
	}

	cloog_domain_free(domain);

	ClastTilingPair &trapezoidalPair = clastTilingPairs[clastTilingPairs.size() - 2];

	log(std::cerr) << "Duplicating outer for loop " << trapezoidalPair.outerFor->iterator << std::endl;
	std::cerr << trapezoidalPair.outerFor << std::endl;
	// TODO: we might have to find a way to group user statement by pair of original and duplicated
	struct clast_stmt *secondTileBand = clast_stmt_hard_copy(&(trapezoidalPair.outerFor->stmt));
	clast_pprint(stderr, secondTileBand, 0, cloogOptions);
	trapezoidalPair.outerFor->stmt.next = secondTileBand;
	log(std::cerr) << "Duplication done" << std::endl;
}