#ifndef PESTO_SPLIT_TILING_PASS_H
#define PESTO_SPLIT_TILING_PASS_H

#include <memory>
#include <vector>

#include <isl/ctx.h>

#include <pesto/ParamTilingUtility.hpp>
#include <pesto/Pass.hpp>

namespace pesto::pass {

class SplitTilingPass : public IPass {
  public:
	SplitTilingPass();
	virtual ~SplitTilingPass();

	virtual void applyScop(OslScop &nest) override;

  private:
	isl_ctx *ctx;
	std::vector<std::unique_ptr<std::string>> stringKeepAlive;
	int insertedStmts;

	void tileClastFor(OslScop &scop, CloogProgram *cloogProgram, CloogOptions *cloogOptions, struct clast_root *root,
					  struct clast_for *clastFor);
};

} // namespace pesto::pass

#endif // PESTO_SPLIT_TILING_PASS_H