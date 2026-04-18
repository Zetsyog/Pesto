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

#include <pesto/ParamTilingUtility.hpp>
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

	struct clast_for *innerFor;
	struct clast_for *outerFor;

	struct clast_user_stmt *outerHeaderStmt;
	struct clast_user_stmt *outerBodyStmt;
	struct clast_user_stmt *innerBodyStmt0; // front 0
	struct clast_user_stmt *innerBodyStmt1; // front 1
};
} // namespace ptile

class TpzTilingPass : public IPass {
  public:
	TpzTilingPass();
	virtual ~TpzTilingPass();

	virtual Bundle apply(Bundle &&bundle) override;
	virtual std::set<std::string> inputIR() const override {
		return {pesto::ir::OpenScopIR::ID};
	}
	virtual std::string outputIR() const override {
		return pesto::ir::ClastIR::ID;
	};

	void setUseMinVolume(bool use) {
		this->useMinVolume = use;
	}

  private:
	isl_ctx *ctx;
	isl_printer *printer;

	std::vector<std::unique_ptr<std::string>> stringKeepAlive;
	int insertedStmts;

	std::filesystem::path outputFile;

	/* options */
	bool useMinVolume = false;
	/* end options*/

	void tileClastFor(Bundle &bundle, struct clast_for *clastFor, pesto::maths::Vec2i slope);

	std::vector<ptile::ClastTilingPair> computeTilingPairs(pesto::ir::ClastIR &clastIR, struct clast_for *clastFor);

	void insertOuterForLoop(pesto::ir::ClastIR &clastIR, std::vector<ptile::ClastTilingPair> &clastTilingPairs);

	__isl_give isl_set *computeInnerDomain(pesto::ir::ClastIR &clastIR,
										   std::vector<ptile::ClastTilingPair> &clastTilingPairs);

	void generateTrahrheHeaders(pesto::bundle::metadata::Macro &macroData, isl_set *innerDomain);

	void codegen(Bundle &bundle, std::vector<ptile::ClastTilingPair> &clastTilingPairs, isl_set *innerDomain,
				 pesto::maths::Vec2i slope);

	void insertInnerLoopBounds(pesto::ir::ClastIR &clastIR, std::vector<ptile::ClastTilingPair> &clastTilingPairs);

	std::pair<std::string, std::string> computeInnerMinMaxStr(isl_set *innerDomain, unsigned dimIdx);

	void insertOmpPragmas(pesto::ir::ClastIR &clastIR, std::vector<ptile::ClastTilingPair> &clastTilingPairs);

	std::optional<pesto::maths::Vec2i> computeDepDistanceVector(osl_scop_p scop);
};

} // namespace pesto::pass
