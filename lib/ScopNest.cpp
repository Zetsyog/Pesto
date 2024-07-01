#include <attila/ScopNest.hpp>

ScopNest::ScopNest(osl_scop_p scop) { this->scop = scop; }

osl_scop_p ScopNest::getScop() { return this->scop; }

ScopNest::~ScopNest() {}