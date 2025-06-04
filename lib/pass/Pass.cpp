#include <string>

#include <pesto/Pass.hpp>

using namespace pesto::pass;

IPass::IPass(std::string name) : name(name) {}

IPass::~IPass() {}

/**
 * Log a message to the given output stream by prepending the pass name between
 * brackets
 * @param os the output stream
 * @return a reference to the output stream
 */
std::ostream &IPass::log(std::ostream &os) { return os << "[" << name << "] "; }

/**
 * Call the applyScop method on each ScopNest of the Program
 * @param prog the Program to run the pass on
 */
void IPass::apply(pesto::Program &prog) {
	std::vector<OslScop *> nests = prog.getScopNests();
	for (auto &nest : nests) {
		this->applyScop(*nest);
	}
}

void IPass::applyScop(pesto::OslScop &nest) { (void)nest; }