#include <attila/Attila.hpp>

void IPolySingleNestPass::run(PolyProgram &prog) {
	// TODO
	std::vector<ScopNest> nests = prog.getScopNests();
	for (ScopNest nest : nests) {
		this->runSingleNest(nest);
	}
}

IPolyhedralPass::~IPolyhedralPass() {}

IPolySingleNestPass::~IPolySingleNestPass() {}