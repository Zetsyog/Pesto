#ifndef PESTO_SCOPNEST_H
#define PESTO_SCOPNEST_H

#include <osl/osl.h>

#include <pesto/Utils.hpp>

namespace pesto {

class ScopNest {
  private:
	int id;
	osl_scop_p scop;

  public:
	MetadataContainer metadata;

	ScopNest(int id, osl_scop_p scop);
	virtual ~ScopNest();

	osl_scop_p getScop();
	int getId();
};

} // namespace pesto

#endif // PESTO_SCOPNEST_H