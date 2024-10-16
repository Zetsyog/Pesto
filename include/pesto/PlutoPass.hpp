#ifndef PESTO_PASS_PLUTO_H
#define PESTO_PASS_PLUTO_H

#include <pesto/OslScop.hpp>
#include <pesto/Pass.hpp>
#include <pesto/Program.hpp>
#include <string>

#include <pluto/pluto.h>

namespace pesto {

namespace pass {

/**
 * A generic pass that runs Pluto on each OslScop of the program
 */
class PlutoPass : public ISinglePass {
  public:
	PlutoPass();
	virtual ~PlutoPass();

	virtual void runSingleNest(OslScop &nest);

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

} // namespace pass

} // namespace pesto

#endif // PESTO_PASS_PLUTO_H