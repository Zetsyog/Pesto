#ifndef PESTO_PB_SPLIT_NEST_H
#define PESTO_PB_SPLIT_NEST_H

#include <pesto/passbuilder/PassBuilder.hpp>

#include <memory>

namespace pesto {

namespace builder {

class SplitNestPB : public IPassBuilder {
  public:
	SplitNestPB();
	virtual ~SplitNestPB();

	virtual std::unique_ptr<pesto::pass::IPass> build(PassManagerBuilder &builder, const json &j);
};

} // namespace builder

} // namespace pesto

#endif // PESTO_PB_SPLIT_NEST_H