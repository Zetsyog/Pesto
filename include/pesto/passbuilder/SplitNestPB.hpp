#ifndef PESTO_PB_SPLIT_NEST_H
#define PESTO_PB_SPLIT_NEST_H

#include <pesto/PassBuilder.hpp>

#include <memory>
#include <string>

namespace pesto {

class SplitNestPB : public IPassBuilder {
  public:
	SplitNestPB();
	virtual ~SplitNestPB();

	virtual std::string getUUID();
	virtual std::unique_ptr<pesto::IPolyhedralPass> build(const json &j);
};

}; // namespace pesto

#endif // PESTO_PB_SPLIT_NEST_H