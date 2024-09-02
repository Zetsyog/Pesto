#ifndef PESTO_PB_CLOOG_CODEGEN_H
#define PESTO_PB_CLOOG_CODEGEN_H

#include <pesto/PassBuilder.hpp>

#include <memory>
#include <string>

namespace pesto {

class CloogCodegenPB : public IPassBuilder {
  public:
	CloogCodegenPB();
	virtual ~CloogCodegenPB();

	virtual std::string getUUID();
	virtual std::unique_ptr<pesto::IPolyhedralPass> build(const json &j);
};

}; // namespace pesto

#endif // PESTO_PB_CLOOG_CODEGEN_H