#ifndef PESTO_PB_PLUTO_H
#define PESTO_PB_PLUTO_H

#include <pesto/PassBuilder.hpp>

#include <memory>
#include <string>

namespace pesto {

class PlutoPB : public IPassBuilder {
  public:
	PlutoPB();
	virtual ~PlutoPB();

	virtual std::unique_ptr<pesto::IPestoPass> build(PMBuilder &builder,
													 const json &j);
};

}; // namespace pesto

#endif // PESTO_PB_PLUTO_H