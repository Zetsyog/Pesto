#ifndef PESTO_PB_GEN_CLAST_H
#define PESTO_PB_GEN_CLAST_H

#include <pesto/PassBuilder.hpp>

#include <memory>
#include <string>

namespace pesto {

class GenClastPB : public IPassBuilder {
  public:
	GenClastPB();
	virtual ~GenClastPB();

	virtual std::string getUUID();
	virtual std::unique_ptr<pesto::IPolyhedralPass> build(const json &j);
};

}; // namespace pesto

#endif // PESTO_PB_GEN_CLAST_H