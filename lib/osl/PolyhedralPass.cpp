#include <pesto/Pesto.hpp>

using namespace pesto;

void IPolySingleNestPass::run(PolyProgram &prog) {
	// TODO
	std::vector<ScopNest *> nests = prog.getScopNests();
	for (auto &nest : nests) {
		this->runSingleNest(*nest);
	}
}

IPolyhedralPass::~IPolyhedralPass() {}

IPolySingleNestPass::~IPolySingleNestPass() {}

std::ostream &IPolyhedralPass::log(std::ostream &os) {
	return os << "[" << this->getUUID() << "] ";
}