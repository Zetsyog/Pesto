#pragma once

#include <pesto/pass/Pass.hpp>
#include <pesto/pass/SkewTransform.hpp>

#include <isl/ctx.h>
#include <osl/osl.h>

#include <string>
#include <vector>

namespace pesto::pass {

// Applies a user-specified skewing to the scattering of a scop so that the
// dependence distance vectors land in the fixed trapezoidal cone (1,0)-(1,1).
// Expects an identity scattering on input (Pluto with identity:true). An
// integrated checker verifies the cone and aborts with a precise message if
// the transform does not qualify.
class SkewPass : public pesto::pass::IPass {
  public:
	SkewPass();
	virtual ~SkewPass();

	void setTransform(const std::vector<std::string> &lines);

	virtual pesto::Bundle apply(pesto::Bundle &&bundle) override;
	virtual std::set<std::string> inputIR() const override;
	virtual std::string outputIR() const override;

  private:
	SkewTransform transform_;
	bool haveTransform_ = false;

	osl_relation_p skewStatementScattering(isl_ctx *ctx, osl_statement_p stmt,
						const std::vector<std::string> &paramNames);
	SkewTransform detectTransform(osl_statement_p stmt);					
	void checkCone(isl_ctx *ctx, osl_scop_p scop);
};

} // namespace pesto::pass

