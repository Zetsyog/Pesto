#include <iostream>
#include <pesto/PassManager.hpp>
#include <pesto/macros.h>

using namespace pesto;

PassManager::PassManager() {}

PassManager::~PassManager() {}

void PassManager::runPasses(PestoProgram &prog) {
	for (auto it = this->passes.begin(); it != this->passes.end(); ++it) {
		int nestCount = prog.getScopNests().size();
		std::cerr << "[" << PESTO_NAME_PM << "] Running pass '"
				  << (*it)->getName() << "' on " << std::to_string(nestCount)
				  << " loop nests " << std::endl;
		(*it)->run(prog);
	}
}

void PassManager::addPass(std::unique_ptr<IPestoPass> pass) {
	this->passes.push_back(std::move(pass));
}
