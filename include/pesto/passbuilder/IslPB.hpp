#ifndef PESTO_PB_ISL_H
#define PESTO_PB_ISL_H

#include <pesto/PassBuilder.hpp>

#include <memory>

namespace pesto {

namespace builder {

class IslPB : public IPassBuilder {
  public:
	IslPB();
	virtual ~IslPB();

	virtual std::unique_ptr<pesto::pass::IPass> build(PassManagerBuilder &builder, const json &j);
};

} // namespace builder

} // namespace pesto

#endif // PESTO_PB_ISL_H