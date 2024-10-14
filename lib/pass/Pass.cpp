#include <pesto/Pesto.hpp>

using namespace pesto;

IPestoPass::IPestoPass(std::string name) : name(name) {}

IPestoPass::~IPestoPass() {}

/**
 * Log a message to the given output stream by prepending the pass name between
 * brackets
 * @param os the output stream
 * @return a reference to the output stream
 */
std::ostream &IPestoPass::log(std::ostream &os) {
	return os << "[" << name << "] ";
}

IPestoSingleNestPass::IPestoSingleNestPass(std::string name)
	: IPestoPass(name) {}

IPestoSingleNestPass::~IPestoSingleNestPass() {}

/**
 * Call the runSingleNest method on each ScopNest of the PestoProgram
 * @param prog the PestoProgram to run the pass on
 */
void IPestoSingleNestPass::run(PestoProgram &prog) {

	std::vector<ScopNest *> nests = prog.getScopNests();
	for (auto &nest : nests) {
		this->runSingleNest(*nest);
	}
}
