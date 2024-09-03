#include <pesto/Pesto.hpp>

using namespace pesto;

PolyProgram::PolyProgram() {}

PolyProgram::~PolyProgram() { this->clearScopNests(); }

/**
 * Add a ScopNest to the PolyProgram
 * @param nest A pointer to the ScopNest object to add
 */
void PolyProgram::addScopNest(ScopNest *nest) { this->nests.push_back(nest); }

void PolyProgram::clearScopNests() {
	for (auto &nest : this->nests) {
		delete nest;
	}
	this->nests.clear();
}

/**
 * Get the ScopNest objects in the PolyProgram
 * @return A vector of pointers to the ScopNest objects
 */
std::vector<ScopNest *> PolyProgram::getScopNests() { return this->nests; }

PolyProgramBuilder::PolyProgramBuilder() : program(nullptr) { this->reset(); }

PolyProgramBuilder::~PolyProgramBuilder() {
	if (this->program != nullptr)
		delete this->program;
}

/**
 * Build the PolyProgram object
 * @return A unique pointer to the PolyProgram object
 */
std::unique_ptr<PolyProgram> PolyProgramBuilder::build() {
	PolyProgram *program = this->program;
	this->program = nullptr;
	this->reset();
	return std::unique_ptr<PolyProgram>(program);
}

/**
 * Add a ScopNest to the PolyProgram from an osl_scop_p structure
 * @param scop The osl_scop_p object to add
 * @return A reference to the PolyProgramBuilder object
 */
PolyProgramBuilder &PolyProgramBuilder::addScop(osl_scop_p scop) {
	ScopNest *nest = new ScopNest(nextNestId++, scop);
	this->program->addScopNest(nest);

	return *this;
}

/**
 * Reset the PolyProgramBuilder object
 */
void PolyProgramBuilder::reset() {
	if (this->program != nullptr) {
		delete this->program;
	}
	this->program = new PolyProgram();
}
