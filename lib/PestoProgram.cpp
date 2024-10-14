#include <pesto/Pesto.hpp>

using namespace pesto;

PestoProgram::PestoProgram() {}

PestoProgram::~PestoProgram() { this->clearScopNests(); }

/**
 * Add a ScopNest to the PestoProgram
 * @param nest A pointer to the ScopNest object to add
 */
void PestoProgram::addScopNest(ScopNest *nest) { this->nests.push_back(nest); }

void PestoProgram::clearScopNests() {
	for (auto &nest : this->nests) {
		delete nest;
	}
	this->nests.clear();
}

/**
 * Get the ScopNest objects in the PestoProgram
 * @return A vector of pointers to the ScopNest objects
 */
std::vector<ScopNest *> PestoProgram::getScopNests() { return this->nests; }

PestoProgramBuilder::PestoProgramBuilder() : program(nullptr) { this->reset(); }

PestoProgramBuilder::~PestoProgramBuilder() {
	if (this->program != nullptr)
		delete this->program;
}

/**
 * Build the PestoProgram object
 * @return A unique pointer to the PestoProgram object
 */
std::unique_ptr<PestoProgram> PestoProgramBuilder::build() {
	PestoProgram *program = this->program;
	this->program = nullptr;
	this->reset();
	return std::unique_ptr<PestoProgram>(program);
}

/**
 * Add a ScopNest to the PestoProgram from an osl_scop_p structure
 * @param scop The osl_scop_p object to add
 * @return A reference to the PestoProgramBuilder object
 */
PestoProgramBuilder &PestoProgramBuilder::addScop(osl_scop_p scop) {
	ScopNest *nest = new ScopNest(nextNestId++, scop);
	this->program->addScopNest(nest);

	return *this;
}

/**
 * Reset the PestoProgramBuilder object
 */
void PestoProgramBuilder::reset() {
	if (this->program != nullptr) {
		delete this->program;
	}
	this->program = new PestoProgram();
}
