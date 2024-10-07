#ifndef PESTO_PLUTO_PASSSES_H
#define PESTO_PLUTO_PASSSES_H

#include <pesto/Pass.hpp>
#include <pesto/PolyProgram.hpp>
#include <pesto/ScopNest.hpp>
#include <string>

#include <pluto/pluto.h>

namespace pesto {

/**
 * A generic pass that runs Pluto on each ScopNest of the program
 */
class PlutoPass : public IPestoSingleNestPass {
  public:
	PlutoPass();
	virtual ~PlutoPass();

	virtual void runSingleNest(ScopNest &nest);

	PlutoOptions *getOptions();

  private:
	PlutoContext *context;
};

/**
 * A pass that runs Pluto without any optimization option
 * like polycc --notile --noparallel --nounroll --noprevector
 */
class PlutoAutoTransformPass : public PlutoPass {
  public:
	PlutoAutoTransformPass();
	virtual ~PlutoAutoTransformPass();
};

/**
 * A pass that runs Pluto with only the tiling option
 * like polycc --tile
 */
class PlutoTilingPass : public PlutoPass {
  public:
	PlutoTilingPass();
	virtual ~PlutoTilingPass();
};

} // namespace pesto

#endif // PESTO_PLUTO_PASSSES_H