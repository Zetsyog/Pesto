#ifndef PESTO_PARAM_TILING_H
#define PESTO_PARAM_TILING_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <isl/set.h>

#include <osl/osl.h>

#include <cloog/cloog.h>

#include <pesto/codegen/ParamTiling.hpp>

namespace pesto {

enum class ParamTilingType { RECTANGULAR, ALGEBRAIC, SCHEDULED };

enum class VolumeCheckTrigger {
	NEVER,
	ALWAYS,
	FIRST_OUTTER_ITERATOR_DEPENDENCY,
};

/**
 * Holds information needed to generate trahrhe code
 * Associate outter tiling for to its corresponding inner tiling for
 * Also holds the tiling depth and the codegen data
 */
struct TilingData {
	struct clast_for *outter_for;
	struct clast_for *inner_for;
	int tiling_depth;
	unsigned int head_stmt_id;
	unsigned int body_stmt_id;
};

struct StmtGroup {
	int id;
	std::vector<struct clast_stmt *> stmts;
	struct clast_for *parent_for;

	/**
	 * A vector of TilingData
	 */
	std::vector<TilingData> tilingData;

	/**
	 * Holds domain information to pass to trahrhe software
	 * First element is the number of algebraically tiled dimensions
	 * Second element is the domain in isl string format
	 */
	std::pair<int, std::string> trahrheDomain;
};

/**
 * A utility class to perform parametric tiling on a given program
 * using both the Polyhedral representation and the clast representation
 */
class ParamTilingUtility {
  public:
	ParamTilingUtility();
	virtual ~ParamTilingUtility();

	/**
	 * @brief Perform a parametric tiling on the given OslScop
	 * Tiling entry point. It uses both the Polyhedral representation and the
	 * clast representation
	 * @param nest[in] the OslScop to apply the tiling on
	 */
	void apply(CloogProgram *cloogProgram, CloogOptions *cloogOptions, struct clast_root *root);

	bool hasError();
	std::string getErrorMessage();

	/**
	 * @brief given a depth, return the tiling type
	 * @param depth the depth of the loop to tile
	 * @return the tiling type
	 */
	ParamTilingType getTilingType(std::size_t depth);

	/**
	 * @brief push a tiling type to the list of tiling types
	 * @param type the tiling type to push
	 */
	void pushTilingType(ParamTilingType type);

	void setIdTrahrhePrefix(int id);

	/* Codegen options */
	void setBoundComputingMethod(ptile::codegen::BoundComputeMethod method);
	void setBoundCorrectionStrategy(ptile::codegen::BoundCorrectionStrategy strategy);
	void setVolumeCheckStrategy(ptile::codegen::VolumeCheckStrategy strategy);
	void setVolumeCheckTrigger(VolumeCheckTrigger trigger);
	void setVolumeCheckThreshold(std::string threshold);

	std::vector<std::shared_ptr<std::string>> getNames();

	std::vector<std::pair<int, std::string>> getStatements();

	std::vector<std::string> getVarList();

	std::vector<std::pair<int, std::string>> getTrahrheDomains();

	void setTileSizeTarget(int tilesize);

  private:
	/* polyhedral stuff */
	isl_ctx *ctx; /* isl context */

	std::vector<ParamTilingType> tilingTypes;

	/* stmt utilities */
	int insertedStmts;
	int tilesizeTarget;

	/* error handling */
	bool error;
	std::string errorMessage;

	/* str utilities */
	std::vector<std::shared_ptr<std::string>> names; /* those string should survive till the end of pesto */

	std::vector<std::pair<int, std::string>> trahrheDomains; /* trahrhe domains */

	/* code gen */
	int id;
	std::vector<std::pair<int, std::string>> statements; /* list of statements id and their content */
	std::vector<std::string> varList;

	ptile::codegen::BoundComputeMethod boundComputeMethod;
	ptile::codegen::BoundCorrectionStrategy boundCorrectionStrategy;
	ptile::codegen::VolumeCheckStrategy volumeCheckStrategy;
	VolumeCheckTrigger volumeCheckTrigger;
	std::string volumeCheckThreshold;

	std::vector<StmtGroup> computeStmtGroups(struct clast_root *root);

	void tileGroup(CloogProgram *cloogProgram, CloogOptions *cloogOptions, struct clast_root *root, StmtGroup &group);

	void associateTilingFor(struct clast_root *root, StmtGroup &group);

	int islFirstTilingDimIdx(StmtGroup &group);

	void insertStatements(CloogOptions *cloogOptions, struct clast_root *root, struct StmtGroup &group,
						  TilingData &data);

	void replaceInnerForBounds(CloogProgram *cloogProgram, struct StmtGroup &group, TilingData &data);

	void replaceOutterForBounds(StmtGroup &group, TilingData &data);

	void generateStatements(CloogProgram *cloogProgram, StmtGroup &group);

	__isl_give isl_set *dropTilingConstraints(__isl_take isl_set *set, struct StmtGroup &group);

	__isl_give isl_set *removeTilingDimensions(__isl_take isl_set *set, struct StmtGroup &group);

	__isl_give isl_set *moveNonTiledDimensionsToParameters(__isl_take isl_set *set, struct StmtGroup &group);

	size_t movedDimensionsCount(CloogProgram *cloogProgram, struct StmtGroup &group, TilingData &data);

	void declareVariable(std::string varName);

	void declareGroupVariables(struct StmtGroup &group);

	void computeTrahrheDomain(CloogProgram *cloogProgram, StmtGroup &group);

	void updateOmpPragma(StmtGroup &group);

	__isl_give isl_set *extractIslInnerTiledDomain(StmtGroup &group, __isl_take isl_set *domain, std::size_t ndim);

	__isl_give isl_set *getInnerDomain(CloogProgram *cloogProgram, StmtGroup &group, TilingData &data,
									   bool moveNonTiledDimensions = true);

	ptile::NameHelper getNameHelper(StmtGroup &group, TilingData &data);
	ptile::NameHelper getTrahrheNameHelper(StmtGroup &group, TilingData &data);

	ptile::codegen::ParamTilingCodegen getCodegen(CloogProgram *cloogProgram, StmtGroup &group, TilingData &data);
};

} // namespace pesto

#endif // PESTO_PARAM_TILING_H