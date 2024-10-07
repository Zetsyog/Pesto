#ifndef PESTO_PB_PARAM_CONTEXT
#define PESTO_PB_PARAM_CONTEXT

#include <pesto/PassBuilder.hpp>

#include <memory>
#include <string>

namespace pesto {

class ParamContextPB : public IPassBuilder {
  public:
	ParamContextPB();
	virtual ~ParamContextPB();

	virtual std::string getUUID();
	virtual std::unique_ptr<pesto::IPestoPass> build(PMBuilder &builder,
													 const json &j);
};

}; // namespace pesto

#endif // PESTO_PB_PARAM_CONTEXT