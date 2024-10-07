#ifndef PESTO_PB_CLAST_PARAM_TILING
#define PESTO_PB_CLAST_PARAM_TILING

#include <pesto/PassBuilder.hpp>

#include <memory>
#include <string>

namespace pesto {

class ClastParamTilingPB : public IPassBuilder {
  public:
	ClastParamTilingPB();
	virtual ~ClastParamTilingPB();

	virtual std::unique_ptr<pesto::IPestoPass> build(PMBuilder &builder,
													 const json &j);
};

}; // namespace pesto

#endif // PESTO_PB_CLAST_PARAM_TILING