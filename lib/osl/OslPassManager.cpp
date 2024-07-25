#include <iostream>
#include <pesto/OslPassManager.hpp>
#include <pesto/macros.h>

using namespace pesto;

OslPassManager::OslPassManager() {}

OslPassManager::~OslPassManager() {}

void OslPassManager::runPasses(PolyProgram &prog) {
	for (auto it = this->passes.begin(); it != this->passes.end(); ++it) {
		int nestCount = prog.getScopNests().size();
		std::cerr << "[" << PESTO_NAME_OPT_OSL << "] Running pass '"
				  << (*it)->getUUID() << "' on " << std::to_string(nestCount)
				  << " loop nests " << std::endl;
		(*it)->run(prog);
	}
}

void OslPassManager::addPass(std::unique_ptr<IPolyhedralPass> pass) {
	this->passes.push_back(std::move(pass));
}
