#include <pesto/ScopNest.hpp>

using namespace pesto;

ScopNest::ScopNest(osl_scop_p scop) {
	this->scop = osl_scop_clone(scop);
	;
}

osl_scop_p ScopNest::getScop() { return this->scop; }

ScopNest::~ScopNest() { osl_scop_free(this->scop); }