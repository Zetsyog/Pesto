#include <pesto/Pesto.hpp>

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

ISinglePass::ISinglePass(std::string name) : IPass(name) {}

ISinglePass::~ISinglePass() {}

/**
 * Call the runSingleNest method on each ScopNest of the Program
 * @param prog the Program to run the pass on
 */
void ISinglePass::run(pesto::Program &prog) {

	std::vector<OslScop *> nests = prog.getScopNests();
	for (auto &nest : nests) {
		this->runSingleNest(*nest);
	}
}
