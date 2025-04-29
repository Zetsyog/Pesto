#ifndef PESTO_PASS_PLUTO_H
#define PESTO_PASS_PLUTO_H

#include <pesto/OslScop.hpp>
#include <pesto/Pass.hpp>
#include <pesto/Pluto.hpp>
#include <pesto/Program.hpp>

#include <pluto/pluto.h>

namespace pesto {

namespace pass {

/**
 * A generic pass that runs Pluto on each OslScop of the program
 */
class PlutoPass : public IPass {
  public:
	PlutoPass();

	virtual void applyScop(OslScop &nest) override;

	PlutoOptions &getOptions();

  private:
	PlutoWrapper pluto;
};

/**
 * A pass that runs Pluto without any optimization option
 * like polycc --notile --noparallel --nounroll --noprevector
 */
class PlutoAutoTransformPass : public PlutoPass {
  public:
	PlutoAutoTransformPass();
};

/**
 * A pass that runs Pluto with only the tiling option
 * like polycc --tile
 */
class PlutoTilingPass : public PlutoPass {
  public:
	PlutoTilingPass();
};

} // namespace pass

} // namespace pesto

#endif // PESTO_PASS_PLUTO_H