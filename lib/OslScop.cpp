#include <pesto/OslScop.hpp>

using namespace pesto;

OslScop::OslScop(int id, osl_scop_p scop) : id(id) { this->scop = osl_scop_clone(scop); }

osl_scop_p OslScop::c_scop() { return this->scop; }

OslScop::~OslScop() { osl_scop_free(this->scop); }

int OslScop::getId() { return this->id; }