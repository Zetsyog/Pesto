#include <pesto/Pesto.hpp>

using namespace pesto;

IPolyhedralPass::IPolyhedralPass(std::string name) : name(name) {}

IPolyhedralPass::~IPolyhedralPass() {}

/**
 * Log a message to the given output stream by prepending the pass name between
 * brackets
 * @param os the output stream
 * @return a reference to the output stream
 */
std::ostream &IPolyhedralPass::log(std::ostream &os) {
	return os << "[" << name << "] ";
}

IPolySingleNestPass::IPolySingleNestPass(std::string name)
	: IPolyhedralPass(name) {}

IPolySingleNestPass::~IPolySingleNestPass() {}

/**
 * Call the runSingleNest method on each ScopNest of the PolyProgram
 * @param prog the PolyProgram to run the pass on
 */
void IPolySingleNestPass::run(PolyProgram &prog) {

	std::vector<ScopNest *> nests = prog.getScopNests();
	for (auto &nest : nests) {
		this->runSingleNest(*nest);
	}
}
