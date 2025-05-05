#ifndef PESTO_PB_CLOOG_CODEGEN_H
#define PESTO_PB_CLOOG_CODEGEN_H

#include <pesto/PassBuilder.hpp>

#include <memory>

namespace pesto {

namespace builder {

class CloogCodegenPB : public IPassBuilder {
  public:
	CloogCodegenPB();
	virtual ~CloogCodegenPB();

	virtual std::unique_ptr<pesto::pass::IPass> build(PassManagerBuilder &builder, const json &j);
};

} // namespace builder

} // namespace pesto

#endif // PESTO_PB_CLOOG_CODEGEN_H