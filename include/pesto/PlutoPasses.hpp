#ifndef PESTO_PLUTO_PASSSES_H
#define PESTO_PLUTO_PASSSES_H

#include <pesto/PolyProgram.hpp>
#include <pesto/PolyhedralPass.hpp>
#include <pesto/ScopNest.hpp>
#include <string>

#include <pluto/pluto.h>

namespace pesto {

class PlutoPass : public IPolySingleNestPass {
  public:
	PlutoPass(std::string suffix);
	virtual ~PlutoPass();

	virtual void runSingleNest(ScopNest &nest);
	virtual std::string getUUID();

	PlutoOptions *getOptions();

  private:
	std::string suffix;
	PlutoContext *context;
};

class PlutoAutoTransformPass : public PlutoPass {
  public:
	PlutoAutoTransformPass();
	virtual ~PlutoAutoTransformPass();
};

class PlutoTilingPass : public PlutoPass {
  public:
	PlutoTilingPass();
	virtual ~PlutoTilingPass();
};

} // namespace pesto

#endif // PESTO_PLUTO_PASSSES_H