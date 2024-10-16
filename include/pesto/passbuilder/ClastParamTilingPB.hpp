#ifndef PESTO_PB_CLAST_PARAM_TILING
#define PESTO_PB_CLAST_PARAM_TILING

#include <pesto/PassBuilder.hpp>

#include <memory>
#include <string>

namespace pesto {

namespace builder {

class ClastParamTilingPB : public IPassBuilder {
  public:
	ClastParamTilingPB();
	virtual ~ClastParamTilingPB();

	virtual std::unique_ptr<pesto::pass::IPass>
	build(PassManagerBuilder &builder, const json &j);
};

} // namespace builder

} // namespace pesto

#endif // PESTO_PB_CLAST_PARAM_TILING