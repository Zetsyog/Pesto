#include <pesto/Pesto.hpp>

using namespace pesto;

Program::~Program() { this->clearScopNests(); }

/**
 * Add a OslScop to the Program
 * @param nest A pointer to the OslScop object to add
 */
void Program::addScopNest(OslScop *nest) { this->scops.push_back(nest); }

void Program::clearScopNests() {
	for (auto &nest : this->scops) {
		delete nest;
	}
	this->scops.clear();
}

/**
 * Get the OslScop objects in the Program
 * @return A vector of pointers to the OslScop objects
 */
std::vector<OslScop *> Program::getScopNests() { return this->scops; }

Program::Builder::Builder() : nextScopId(0) {}

/**p
 * Build the Program object
 * @return A unique pointer to the Program object
 */
std::unique_ptr<pesto::Program> Program::Builder::build() {
	auto program = std::make_unique<pesto::Program>();
	for (auto scop : this->scops) {
		program->addScopNest(scop);
	}
	this->scops.clear();
	return program;
}

/**
 * Add a OslScop to the Program from an osl_scop_p structure
 * @param scop The osl_scop_p object to add
 * @return A reference to the ProgramBuilder object
 */
Program::Builder &Program::Builder::addScop(osl_scop_p scop) {
	OslScop *nest = new OslScop(nextScopId++, scop);
	this->scops.push_back(nest);

	return *this;
}
