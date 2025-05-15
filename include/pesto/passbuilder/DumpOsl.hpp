#ifndef PESTO_PB_OSL_DUMP_H
#define PESTO_PB_OSL_DUMP_H

#include <pesto/PassBuilder.hpp>

#include <memory>

namespace pesto {

namespace builder {

class OslDumpPB : public IPassBuilder {
  public:
	OslDumpPB();
	virtual ~OslDumpPB();

	virtual std::unique_ptr<pesto::pass::IPass> build(PassManagerBuilder &builder, const json &j);
};

} // namespace builder

} // namespace pesto

#endif // PESTO_PB_OSL_DUMP_H