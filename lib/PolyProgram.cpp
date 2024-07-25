#include <pesto/Pesto.hpp>

using namespace pesto;

PolyProgram::PolyProgram() {}

PolyProgram::~PolyProgram() { this->clearScopNests(); }

void PolyProgram::addScopNest(ScopNest *nest) { this->nests.push_back(nest); }

void PolyProgram::clearScopNests() {
	for (auto &nest : this->nests) {
		delete nest;
	}
	this->nests.clear();
}

std::vector<ScopNest *> PolyProgram::getScopNests() { return this->nests; }

PolyProgramBuilder::PolyProgramBuilder() : program(nullptr) { this->reset(); }

PolyProgramBuilder::~PolyProgramBuilder() {
	if (this->program != nullptr)
		delete this->program;
}

std::unique_ptr<PolyProgram> PolyProgramBuilder::build() {
	PolyProgram *program = this->program;
	this->program = nullptr;
	this->reset();
	return std::unique_ptr<PolyProgram>(program);
}

PolyProgramBuilder &PolyProgramBuilder::addScop(osl_scop_p scop) {
	ScopNest *nest = new ScopNest(scop);
	this->program->addScopNest(nest);

	return *this;
}

void PolyProgramBuilder::reset() {
	if (this->program != nullptr) {
		delete this->program;
	}
	this->program = new PolyProgram();
}
