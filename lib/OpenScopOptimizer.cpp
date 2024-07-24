#include <iostream>
#include <pesto/OpenScopOptimizer.hpp>

OpenScopOptimizer::OpenScopOptimizer() {}

OpenScopOptimizer::~OpenScopOptimizer() {}

void OpenScopOptimizer::runPasses(PolyProgram &prog) {
	for (auto it = this->passes.begin(); it != this->passes.end(); ++it) {
		int nestCount = prog.getScopNests().size();
		std::cerr << "[pesto-optimizer] Running pass " << (*it)->getUUID()
				  << " on " << std::to_string(nestCount) << " loop nests "
				  << std::endl;
		(*it)->run(prog);
	}
}

void OpenScopOptimizer::addPass(IPolyhedralPass *pass) {
	this->passes.push_back(pass);
}
