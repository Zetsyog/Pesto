#ifndef PESTO_PB_CLAST_CUSTOM_VEC_PRAGMA_H
#define PESTO_PB_CLAST_CUSTOM_VEC_PRAGMA_H

#include <pesto/passbuilder/PassBuilder.hpp>

#include <memory>
#include <string>

namespace pesto {

namespace builder {

class ClastCustomVecPragmaPB : public IPassBuilder {
  public:
	ClastCustomVecPragmaPB();
	virtual ~ClastCustomVecPragmaPB();

	virtual std::unique_ptr<pesto::pass::IPass> build(PassManagerBuilder &builder, const json &j);
};

} // namespace builder

} // namespace pesto

#endif // PESTO_PB_CLAST_CUSTOM_VEC_PRAGMA_H