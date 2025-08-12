#ifndef PESTO_PB_SPLIT_TILING_H
#define PESTO_PB_SPLIT_TILING_H

#include <pesto/passbuilder/PassBuilder.hpp>

#include <memory>

namespace pesto {

namespace builder {

class TpzTilingPB : public IPassBuilder {
  public:
	TpzTilingPB();
	virtual ~TpzTilingPB();

	virtual std::unique_ptr<pesto::pass::IPass> build(PassManagerBuilder &builder, const json &j);
};

} // namespace builder

} // namespace pesto

#endif // PESTO_PB_SPLIT_TILING_H