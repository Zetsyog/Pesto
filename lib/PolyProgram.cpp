#include <attila/Attila.hpp>

PolyProgram::PolyProgram() {}

PolyProgram::~PolyProgram() {}

void PolyProgram::addScopNest(ScopNest nest) { this->nests.push_back(nest); }

void PolyProgram::clearScopNests() { this->nests.clear(); }

std::vector<ScopNest> PolyProgram::getScopNests() { return this->nests; }

PolyProgramBuilder::PolyProgramBuilder() : program(nullptr) { this->reset(); }

PolyProgramBuilder::~PolyProgramBuilder() {
	if (this->program != nullptr)
		delete this->program;
}

PolyProgram *PolyProgramBuilder::build() {
	PolyProgram *program = this->program;
	this->program = nullptr;
	this->reset();
	return program;
}

void PolyProgramBuilder::addScop(osl_scop_p scop) {
	ScopNest nest(scop);
	this->program->addScopNest(nest);
}

void PolyProgramBuilder::reset() {
	if (this->program != nullptr) {
		delete this->program;
	}
	this->program = new PolyProgram();
}
