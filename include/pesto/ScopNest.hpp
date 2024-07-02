#ifndef PESTO_SCOPNEST_H
#define PESTO_SCOPNEST_H

#include <osl/osl.h>

class ScopNest {
  private:
	osl_scop_p scop;

  public:
	ScopNest(osl_scop_p scop);
	virtual ~ScopNest();

	osl_scop_p getScop();
};

#endif // PESTO_SCOPNEST_H