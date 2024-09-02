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
	PlutoPass();
	virtual ~PlutoPass();

	virtual void runSingleNest(ScopNest &nest);
	virtual std::string getUUID();

	PlutoOptions *getOptions();

  private:
	PlutoContext *context;
};

class PlutoAutoTransformPass : public PlutoPass {
  public:
	PlutoAutoTransformPass();
	virtual ~PlutoAutoTransformPass();
	virtual std::string getUUID();
};

class PlutoTilingPass : public PlutoPass {
  public:
	PlutoTilingPass();
	virtual ~PlutoTilingPass();
	virtual std::string getUUID();
};

} // namespace pesto

#endif // PESTO_PLUTO_PASSSES_H