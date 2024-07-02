#include <iostream>
#include <pesto/OpenScopOptimizer.hpp>

OpenScopOptimizer::OpenScopOptimizer() {}

OpenScopOptimizer::~OpenScopOptimizer() {}

void OpenScopOptimizer::runPasses(PolyProgram &prog) {
	for (auto &pass : this->passes) {
		int nestCount = prog.getScopNests().size();
		std::cerr << "[pesto-optimizer] Running pass " << pass->getUUID()
				  << " on " << std::to_string(nestCount) << " loop nests "
				  << std::endl;
		pass->run(prog);
	}
}

void OpenScopOptimizer::addPass(IPolyhedralPass *pass) {
	this->passes.push_back(pass);
}
