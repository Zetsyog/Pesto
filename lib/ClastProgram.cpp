#include <attila/ClastNest.hpp>
#include <attila/ClastProgram.hpp>
#include <cloog/cloog.h>

ClastProgram::ClastProgram() {}

ClastProgram::~ClastProgram() {
	for (auto &nest : this->nests)
		delete nest;
}

void ClastProgram::addNest(ClastNest *nest) {
	if (nest == nullptr) {
		return;
	}
	this->nests.push_back(nest);
}

std::vector<ClastNest *> ClastProgram::getNests() { return this->nests; }

ClastProgramBuilder::ClastProgramBuilder() : program(nullptr) { this->reset(); }

ClastProgramBuilder::~ClastProgramBuilder() {
	if (this->program != nullptr)
		delete this->program;
}

ClastProgram *ClastProgramBuilder::build() {
	ClastProgram *program = this->program;
	this->program = nullptr;
	this->reset();
	return program;
}

void ClastProgramBuilder::addNest(ScopNest &nest) {
	ClastNest *clastNest = new ClastNest();

	osl_scop_p scop = nest.getScop();
	CloogInput *input =
		cloog_input_from_osl_scop(clastNest->getCloogState(), scop);
	struct clast_stmt *root =
		cloog_clast_create_from_input(input, clastNest->getCloogOptions());

	clastNest->setClastRoot(root);
	clastNest->getCloogOptions()->f = -1;
	clastNest->getCloogOptions()->l = -1;
	clastNest->getCloogOptions()->compilable = 0;
	clastNest->getCloogOptions()->esp = 1;
	clastNest->getCloogOptions()->strides = 1;
	clastNest->getCloogOptions()->sh = 1;
	clastNest->getCloogOptions()->backtrack = 0;

	this->program->addNest(clastNest);
}

void ClastProgramBuilder::fromPolyProgram(PolyProgram *program) {
	std::vector<ScopNest> nests = program->getScopNests();
	for (auto &nest : nests) {
		this->addNest(nest);
	}
}

void ClastProgramBuilder::reset() {
	if (this->program != nullptr)
		delete this->program;
	this->program = new ClastProgram();
}
