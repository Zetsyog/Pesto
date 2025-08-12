#ifndef PESTO_SPLIT_TILING_PASS_H
#define PESTO_SPLIT_TILING_PASS_H

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <isl/ctx.h>

#include <pesto/ParamTilingUtility.hpp>
#include <pesto/Pass.hpp>
#include <pesto/maths/Maths.hpp>

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

class SplitTilingPass : public IPass {
  public:
	SplitTilingPass();
	virtual ~SplitTilingPass();

	virtual void apply(Program &prog) override;
	virtual void applyScop(OslScop &nest) override;

  private:
	isl_ctx *ctx;
	isl_printer *printer;

	std::vector<std::unique_ptr<std::string>> stringKeepAlive;
	int insertedStmts;

	std::filesystem::path outputFile;

	void tileClastFor(OslScop &scop, CloogProgram *cloogProgram, CloogOptions *cloogOptions, struct clast_root *root,
					  struct clast_for *clastFor, pesto::maths::Vec2i slope);

	std::vector<ptile::ClastTilingPair> computeTilingPairs(OslScop &scop, CloogProgram *cloogProgram,
														   CloogOptions *cloogOptions, struct clast_root *root,
														   struct clast_for *clastFor);
	void insertOuterForLoop(OslScop &scop, struct clast_root *root,
							std::vector<ptile::ClastTilingPair> &clastTilingPairs, CloogProgram *cloogProgram,
							CloogOptions *cloogOptions);

	__isl_give isl_set *computeInnerDomain(OslScop &scop, CloogProgram *cloogProgram, CloogOptions *cloogOptions,
										   std::vector<ptile::ClastTilingPair> &clastTilingPairs);

	void generateTrahrheHeaders(OslScop &scop, CloogProgram *cloogProgram, CloogOptions *cloogOptions,
								struct clast_root *root, std::vector<ptile::ClastTilingPair> &clastTilingPairs,
								isl_set *innerDomain);

	void codegen(OslScop &scop, CloogProgram *cloogProgram, CloogOptions *cloogOptions, struct clast_root *root,
				 std::vector<ptile::ClastTilingPair> &clastTilingPairs, isl_set *innerDomain,
				 pesto::maths::Vec2i slope);

	void insertInnerLoopBounds(OslScop &scop, CloogProgram *cloogProgram,
							   std::vector<ptile::ClastTilingPair> &clastTilingPairs);

	std::pair<std::string, std::string> computeInnerMinMaxStr(OslScop &scop, isl_set *innerDomain, unsigned dimIdx);

	void insertOmpPragmas(CloogProgram *cloogProgram, std::vector<ptile::ClastTilingPair> &clastTilingPairs);

	std::optional<pesto::maths::Vec2i> computeDepDistanceVector(OslScop &scop);
};

} // namespace pesto::pass

#endif // PESTO_SPLIT_TILING_PASS_H