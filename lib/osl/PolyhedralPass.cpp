#include <pesto/Pesto.hpp>

using namespace pesto;

IPolyhedralPass::IPolyhedralPass(std::string name) : name(name) {}

IPolyhedralPass::~IPolyhedralPass() {}

std::ostream &IPolyhedralPass::log(std::ostream &os) {
	return os << "[" << name << "] ";
}

IPolySingleNestPass::IPolySingleNestPass(std::string name)
	: IPolyhedralPass(name) {}

IPolySingleNestPass::~IPolySingleNestPass() {}

void IPolySingleNestPass::run(PolyProgram &prog) {

	std::vector<ScopNest *> nests = prog.getScopNests();
	for (auto &nest : nests) {
		this->runSingleNest(*nest);
	}
}
