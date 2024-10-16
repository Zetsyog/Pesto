#include <pesto/Pesto.hpp>

using namespace pesto;

Program::Program() {}

Program::~Program() { this->clearScopNests(); }

/**
 * Add a OslScop to the Program
 * @param nest A pointer to the OslScop object to add
 */
void Program::addScopNest(OslScop *nest) { this->nests.push_back(nest); }

void Program::clearScopNests() {
	for (auto &nest : this->nests) {
		delete nest;
	}
	this->nests.clear();
}

/**
 * Get the OslScop objects in the Program
 * @return A vector of pointers to the OslScop objects
 */
std::vector<OslScop *> Program::getScopNests() { return this->nests; }

builder::Program::Program() : program(nullptr) { this->reset(); }

builder::Program::~Program() {
	if (this->program != nullptr)
		delete this->program;
}

/**
 * Build the Program object
 * @return A unique pointer to the Program object
 */
std::unique_ptr<pesto::Program> builder::Program::build() {
	pesto::Program *program = this->program;
	this->program = nullptr;
	this->reset();
	return std::unique_ptr<pesto::Program>(program);
}

/**
 * Add a OslScop to the Program from an osl_scop_p structure
 * @param scop The osl_scop_p object to add
 * @return A reference to the ProgramBuilder object
 */
builder::Program &builder::Program::addScop(osl_scop_p scop) {
	OslScop *nest = new OslScop(nextNestId++, scop);
	this->program->addScopNest(nest);

	return *this;
}

/**
 * Reset the ProgramBuilder object
 */
void builder::Program::reset() {
	if (this->program != nullptr) {
		delete this->program;
	}
	this->program = new pesto::Program();
}
