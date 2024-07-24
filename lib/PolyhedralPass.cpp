#include <pesto/Pesto.hpp>

void IPolySingleNestPass::run(PolyProgram &prog) {
	// TODO
	std::vector<ScopNest *> nests = prog.getScopNests();
	for (auto &nest : nests) {
		this->runSingleNest(*nest);
	}
}

IPolyhedralPass::~IPolyhedralPass() {}

IPolySingleNestPass::~IPolySingleNestPass() {}