#include <pesto/ScopNest.hpp>

using namespace pesto;

ScopNest::ScopNest(int id, osl_scop_p scop) : id(id) {
	this->scop = osl_scop_clone(scop);
}

ScopNest::ScopNest(ScopNest &nest) : id(nest.id) {
	this->scop = osl_scop_clone(nest.scop);
}

osl_scop_p ScopNest::getScop() { return this->scop; }

ScopNest::~ScopNest() { osl_scop_free(this->scop); }

int ScopNest::getId() { return this->id; }