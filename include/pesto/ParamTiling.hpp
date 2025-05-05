#ifndef PESTO_PARAM_TILING_H
#define PESTO_PARAM_TILING_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <isl/set.h>

#include <osl/osl.h>

#include <cloog/cloog.h>

#include <cloog/clast.h>

#include <pesto/codegen/ParamTiling.hpp>
#include <pesto/trahrhe.h>
namespace pesto {

enum class ParamTilingType { RECTANGULAR, ALGEBRAIC, SCHEDULED };

/**
 * Holds information needed to generate trahrhe code
 * Associate outter tiling for to its corresponding inner tiling for
 * Also holds the tiling depth and the codegen data
 */
struct TilingData {
	struct clast_for *outter_for;
	struct clast_for *inner_for;
	int tiling_depth;
	// TODO: only one codegen_data should be needed
	std::pair<struct trahrhe_codegen_data, struct trahrhe_codegen_data> codegen_data;
};

struct StmtGroup {
	int id;
	std::vector<struct clast_stmt *> stmts;
	struct clast_for *parent_for;

	/**
	 * A vector of TilingData
	 */
	std::vector<std::unique_ptr<struct TilingData>> tilingData;

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
	void apply();

	bool hasError();
	std::string getErrorMessage();

	void setClastRoot(struct clast_root *root);

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

	void setScop(osl_scop_p scop);
	void setCloogProgram(CloogProgram *cloogProgram);
	void setCloogOptions(CloogOptions *cloogOptions);

	void setIdTrahrhePrefix(int id);

	void setBoundComputingMethod(trahrhe_bound_compute_method method);

	std::vector<std::shared_ptr<std::string>> getNames();

	std::vector<std::pair<int, std::string>> getStatements();

	std::vector<std::string> getVarList();

	std::vector<std::pair<int, std::string>> getTrahrheDomains();

	void setTileSizeTarget(int tilesize);

  private:
	/* polyhedral stuff */
	// TODO: remove this
	osl_scop_p scop; /* scop that will be tiled */
	isl_ctx *ctx;	 /* isl context */

	/* clast stuff */
	struct clast_root *root;
	CloogProgram *cloogProgram;
	CloogOptions *cloogOptions;

	std::vector<std::unique_ptr<struct StmtGroup>> stmtGroups;

	std::vector<ParamTilingType> tilingTypes;

	/* stmt utilities */
	int insertedStmts;
	int tilesizeTarget;

	/* error handling */
	bool error;
	std::string errorMessage;

	/* str utilities */
	std::vector<std::shared_ptr<std::string>> names; /* those string should survive till code gen */

	/* code gen */
	int id;
	std::vector<std::pair<int, std::string>> statements; /* list of statements id and their content */
	std::vector<std::string> varList;

	trahrhe_bound_compute_method boundComputeMethod;

	bool canBeTiled();

	void computeStmtGroups();

	void tileGroup(StmtGroup &group);

	void associateTilingFor(StmtGroup &group);

	void sanityCheck();

	void computeCodegenData(struct StmtGroup &group, struct clast_for *outter_for, struct clast_for *inner_for,
							int tiling_depth);

	int islFirstTilingDimIdx(struct StmtGroup &group);

	void insertStatements(struct StmtGroup &group, struct clast_for *outter_for, struct clast_for *inner_for,
						  int tiling_depth);

	void replaceInnerForBounds(struct StmtGroup &group, struct clast_for *outter_for, struct clast_for *inner_for,
							   int tiling_depth);

	void replaceOutterForBounds(struct StmtGroup &group, struct clast_for *outter_for, struct clast_for *inner_for,
								int tiling_depth);

	void generateStatements(struct StmtGroup &group);

	__isl_give isl_set *dropTilingConstraints(__isl_take isl_set *set, struct StmtGroup &group);

	__isl_give isl_set *removeTilingDimensions(__isl_take isl_set *set, struct StmtGroup &group);

	__isl_give isl_set *moveNonTiledDimensionsToParameters(__isl_take isl_set *set, struct StmtGroup &group);

	void declareVariable(std::string varName);

	void declareGroupVariables(struct StmtGroup &group);

	void computeTrahrheDomain(struct StmtGroup &group);

	void updateOmpPragma(struct StmtGroup &group);

	__isl_give isl_set *extractIslInnerTiledDomain(struct StmtGroup &group, __isl_take isl_set *domain,
												   std::size_t ndim);
};

} // namespace pesto

#endif // PESTO_PARAM_TILING_H