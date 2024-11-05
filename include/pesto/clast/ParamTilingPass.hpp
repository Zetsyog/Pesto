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
class ClastParamTilingPass : public IPass {
  public:
	bool gen_trahrhe;
	std::string trahrhe_mask;

	ClastParamTilingPass();
	virtual ~ClastParamTilingPass();

	virtual void applyScop(pesto::OslScop &nest) override;

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
	std::unique_ptr<GenClastPass> gen_clast_pass;

	std::vector<ParamTilingType> tiling_types;
};

} // namespace pass

} // namespace pesto

#endif // PESTO_PARAM_TILING_PASS_H