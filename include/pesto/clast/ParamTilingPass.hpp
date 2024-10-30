#ifndef PESTO_PARAM_TILING_PASS_H
#define PESTO_PARAM_TILING_PASS_H

#include <pesto/Pass.hpp>
#include <pesto/PlutoPass.hpp>
#include <pesto/clast/GenClastPass.hpp>
#include <pesto/trahrhe.h>

#include <isl/set.h>

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
	struct trahrhe_codegen_data gen_data[2];
};

enum class ParamTilingType { RECTANGULAR, ALGEBRAIC, SCHEDULED };

/**
 * A pass that performs a parametric tiling over the Clast of each OslScop
 */
class ClastParamTilingPass : public ISinglePass {
  public:
	bool gen_trahrhe;
	std::string trahrhe_mask;

	ClastParamTilingPass();
	virtual ~ClastParamTilingPass();

	virtual void run(pesto::Program &prog);
	virtual void runSingleNest(pesto::OslScop &nest);

	void setPlutoPass(std::unique_ptr<IPass> pluto_pass);

	/**
	 * @brief given a depth, return the tiling type
	 * @param depth the depth of the loop to tile
	 * @return the tiling type
	 */
	ParamTilingType getTilingType(int depth);

	/**
	 * @brief push a tiling type to the list of tiling types
	 * @param type the tiling type to push
	 */
	void pushTilingType(ParamTilingType type);

  private:
	bool is_stencil;
	std::unique_ptr<IPass> pluto_pass;
	std::unique_ptr<PlutoPass> tiling_pass;
	std::unique_ptr<GenClastPass> gen_clast_pass;

	std::vector<ParamTilingType> tiling_types;

	void algebraicScheduleCodegen(OslScop &nest, struct tiling_info &current,
								  struct tiling_info &prev);
	void algebraicScheduleHeaderGen(OslScop &nest, tiling_info &current,
									struct tiling_info &prev);

	void compute_trahrhe_isl_domain(
		OslScop &nest, struct StmtGroup *group,
		std::vector<struct tiling_info> &tiling_info_list);

	void applyTiling(OslScop &nest, struct clast_stmt *root,
					 struct StmtGroup *group);
	void
	insert_tiling_statements(OslScop &nest, struct clast_stmt *root,
							 struct tiling_info &tiling_info,
							 std::vector<struct tiling_info> &tiling_info_list);

	void replace_inner_tiled_for_bounds(
		metadata::ClastHolder &metadata, struct clast_stmt *root,
		struct tiling_info &tiling_info,
		std::vector<struct tiling_info> &tiling_info_list);

	void replace_outter_tiled_for_bounds(metadata::ClastHolder &metadata,
										 struct tiling_info &tiling_info);

	void generate_stmt_macros(OslScop &nest, struct tiling_info &tiling_info);
	void addVarDecls(OslScop &nest, struct tiling_info &tiling_info);

	__isl_give isl_set *
	removeTilingDimensions(__isl_take isl_set *set,
						   std::vector<struct tiling_info> &tiling_info_list);

	__isl_give isl_set *
	dropTilingConstraints(__isl_take isl_set *set,
						  std::vector<struct tiling_info> &tiling_info_list);
};

} // namespace pass

} // namespace pesto

#endif // PESTO_PARAM_TILING_PASS_H