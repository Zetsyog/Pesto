#include "pesto/Program.hpp"
#include <memory>
#include <pesto/Pesto.hpp>
#include <utility>

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

std::filesystem::path Program::getInputFile() { return this->inputFile; }

std::filesystem::path Program::getOutputFile() { return this->outputFile; }

Program::Builder::Builder() : nextScopId(0) { this->prog = std::make_unique<Program>(); }

/**p
 * Build the Program object
 * @return A unique pointer to the Program object
 */
std::unique_ptr<pesto::Program> Program::Builder::build() {
	auto builtProgram = std::move(this->prog);
	this->prog = std::make_unique<Program>();
	nextScopId = 0;
	return builtProgram;
}

/**
 * Add a OslScop to the Program from an osl_scop_p structure
 * @param scop The osl_scop_p object to add
 * @return A reference to the ProgramBuilder object
 */
Program::Builder &Program::Builder::addScop(osl_scop_p scop) {
	OslScop *nest = new OslScop(nextScopId++, scop);
	this->prog->addScopNest(nest);
	return *this;
}

Program::Builder &Program::Builder::setInputFile(std::filesystem::path path) {
	this->prog->inputFile = path;
	return *this;
}

Program::Builder &Program::Builder::setOutputFile(std::filesystem::path path) {
	this->prog->outputFile = path;
	return *this;
}