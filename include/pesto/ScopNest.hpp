#ifndef PESTO_SCOPNEST_H
#define PESTO_SCOPNEST_H

#include <osl/osl.h>

#include <pesto/Utils.hpp>

namespace pesto {

/**
 * A class abstracting a program in a polyhedral representation
 * This class is a wrapper around an osl_scop_p object
 * It also contains a unique identifier
 * It can also contain metadata used and / or shared by passes
 */
class ScopNest {
  private:
	int id;
	osl_scop_p scop;

  public:
	MetadataContainer metadata;

	ScopNest(int id, osl_scop_p scop);
	ScopNest(ScopNest &nest);
	virtual ~ScopNest();

	osl_scop_p getScop();
	int getId();
};

} // namespace pesto

#endif // PESTO_SCOPNEST_H