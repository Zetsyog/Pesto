/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <isl/ctx.h>
#include <isl/printer.h>
#include <isl/set.h>

#include <pesto/ir/ClastIR.hpp>
#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/maths/Maths.hpp>
#include <pesto/metadata/Macro.hpp>
#include <pesto/metadata/VarDecl.hpp>
#include <pesto/pass/Pass.hpp>

namespace pesto::pass {

namespace ptile {
class ClastTilingPair {
  public:
	unsigned int depth; // tiling depth
	
	bool underGap = false; // tile loop placed inside t2 instead of at the top

	struct clast_for *innerFor;
	struct clast_for *outerFor;

	struct clast_user_stmt *outerHeaderStmt = nullptr;
	struct clast_user_stmt *outerBodyStmt = nullptr;
	std::vector<struct clast_user_stmt *> innerBodyStmts0;
	std::vector<struct clast_user_stmt *> innerBodyStmts1;
	struct clast_user_stmt *innerBodyStmt0 = nullptr; // == innerBodyStmts0[0], kept for the null checks
	struct clast_user_stmt *innerBodyStmt1 = nullptr;
};
} // namespace ptile

class TpzTilingPass : public IPass {
  public:
	TpzTilingPass();
	virtual ~TpzTilingPass();

	virtual Bundle apply(Bundle &&bundle) override;

	virtual std::set<std::string> inputIR() const override {
		return {pesto::ir::OpenScopIR::getID()};
	}
	virtual std::string outputIR() const override {
		return pesto::ir::ClastIR::getID();
	};

	void setUseMinVolume(bool use) {
		this->useMinVolume = use;
	}

  private:
	isl_ctx *ctx;
	isl_printer *printer;

	std::vector<std::unique_ptr<std::string>> stringKeepAlive;
	int insertedStmts;

	/* options */
	bool useMinVolume = false;
	/* end options*/

	void tileClastFor(Bundle &bundle, struct clast_for *clastFor, pesto::maths::Vec2i slope);

	std::vector<ptile::ClastTilingPair> computeTilingPairs(pesto::ir::ClastIR &clastIR, struct clast_for *clastFor);

	void insertOuterForLoop(pesto::Bundle &bundle, std::vector<ptile::ClastTilingPair> &clastTilingPairs);

	__isl_give isl_set *computeInnerDomain(pesto::ir::ClastIR &clastIR,
										   std::vector<ptile::ClastTilingPair> &clastTilingPairs);

	void generateTrahrheHeaders(pesto::Bundle &bundle, isl_set *innerDomain);

	void codegen(Bundle &bundle, std::vector<ptile::ClastTilingPair> &clastTilingPairs, isl_set *innerDomain,
				 pesto::maths::Vec2i slope);

	void insertInnerLoopBounds(pesto::ir::ClastIR &clastIR, std::vector<ptile::ClastTilingPair> &clastTilingPairs);

	std::pair<std::string, std::string> computeInnerMinMaxStr(isl_set *innerDomain, unsigned dimIdx);

	void insertOmpPragmas(pesto::ir::ClastIR &clastIR, std::vector<ptile::ClastTilingPair> &clastTilingPairs);

	std::optional<pesto::maths::Vec2i> computeDepDistanceVector(osl_scop_p scop);
	
    	bool zTileable_ = true;
    	bool isReductionTileable(osl_scop_p scop);
    	bool hoistingLegal_ = true;
	bool isHoistingLegal(osl_scop_p scop);
	bool underGapLegal_ = true;
};

} // namespace pesto::pass
