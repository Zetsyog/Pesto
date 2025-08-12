#ifndef PESTO_PB_CONDITIONAL_PASS_H
#define PESTO_PB_CONDITIONAL_PASS_H

#include <pesto/passbuilder/PassBuilder.hpp>

namespace pesto {

namespace builder {

class MatchCriteriaPB : public IPassBuilder {
  public:
	MatchCriteriaPB();
	virtual ~MatchCriteriaPB();

	virtual std::unique_ptr<pesto::pass::IPass> build(PassManagerBuilder &builder, const json &j);
};

} // namespace builder

} // namespace pesto

#endif // PESTO_PB_CONDITIONAL_PASS_H
