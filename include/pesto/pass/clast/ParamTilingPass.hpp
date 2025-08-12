#ifndef PESTO_PARAM_TILING_PASS_H
#define PESTO_PARAM_TILING_PASS_H

#include <filesystem>
#include <optional>

#include <pesto/ParamTilingUtility.hpp>
#include <pesto/Program.hpp>
#include <pesto/pass/Pass.hpp>
#include <pesto/pass/PlutoPass.hpp>
#include <pesto/pass/clast/GenClastPass.hpp>

#include <isl/set.h>

#define PESTO_PARAM_TILING_DEFAULT_TILE_SIZE_HINT 32

namespace pesto {

namespace pass {

struct StmtGroup {
	int id;
	std::vector<struct clast_stmt *> stmts;
	struct clast_for *parent_for;
};

struct tiling_info {
	int depth;
	struct clast_for *inner_for;
	struct clast_for *outter_for;
};

/**
 * A pass that performs a parametric tiling over the Clast of each OslScop
 */
class ClastParamTilingPass : public IPass {
  public:
	bool gen_trahrhe;
	bool trahrhe_use_dichotomy;

	ClastParamTilingPass();
	virtual ~ClastParamTilingPass();

	virtual void applyScop(pesto::OslScop &nest) override;
	virtual void apply(pesto::Program &prog) override;

	/**
	 * @brief given a depth, return the tiling type
	 * @param depth the depth of the loop to tile
	 * @return the tiling type
	 */
	pesto::ParamTilingType getTilingType(size_t depth);

	/**
	 * @brief push a tiling type to the list of tiling types
	 * @param type the tiling type to push
	 */
	void pushTilingType(pesto::ParamTilingType type);

	void setTileSizeHint(int size);
	void setBoundComputeMethod(ptile::codegen::BoundComputeMethod method);
	void setBoundCorrectionStrategy(ptile::codegen::BoundCorrectionStrategy strategy);
	void setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy strategy);
	void setVolumeCheckTrigger(VolumeCheckTrigger trigger);
	void setVolumeCheckThreshold(std::string threshold);

  private:
	std::unique_ptr<GenClastPass> gen_clast_pass;
	std::vector<pesto::ParamTilingType> tiling_types;
	std::filesystem::path outputFile;
	ParamTilingUtility ptileUtil;

	bool areTileSkewed(pesto::OslScop &scop);
	std::optional<std::pair<int, int>> getSkewedTilingDimensions(osl_statement_p stmt);

	void undoTileSkewing(pesto::OslScop &scop);
};

} // namespace pass

} // namespace pesto

#endif // PESTO_PARAM_TILING_PASS_H