#include <attila/OpenScopOptimizer.hpp>
#include <iostream>

OpenScopOptimizer::OpenScopOptimizer() {}

OpenScopOptimizer::~OpenScopOptimizer() {}

void OpenScopOptimizer::runPasses(PolyProgram &prog) {
	for (auto &pass : this->passes) {
		std::cerr << "[attila-optimizer] Running pass " << pass->getUUID()
				  << std::endl;
		pass->run(prog);
	}
}

void OpenScopOptimizer::addPass(IPolyhedralPass *pass) {
	this->passes.push_back(pass);
}
