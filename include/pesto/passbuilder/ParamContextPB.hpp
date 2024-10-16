#ifndef PESTO_PB_PARAM_CONTEXT
#define PESTO_PB_PARAM_CONTEXT

#include <pesto/PassBuilder.hpp>

#include <memory>
#include <string>

namespace pesto {

namespace builder {

class ParamContextPB : public IPassBuilder {
  public:
	ParamContextPB();
	virtual ~ParamContextPB();

	virtual std::unique_ptr<pesto::pass::IPass>
	build(PassManagerBuilder &builder, const json &j);
};

} // namespace builder

} // namespace pesto

#endif // PESTO_PB_PARAM_CONTEXT