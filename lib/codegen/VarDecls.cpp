#include <algorithm>

#include <pesto/CodegenPasses.hpp>

using namespace pesto;

VarDeclMetadata::VarDeclMetadata() : type("long int") {}

VarDeclMetadata::~VarDeclMetadata() {}

void VarDeclMetadata::addVar(std::string var) {
	if (std::find(vars.begin(), vars.end(), var) != vars.end()) {
		return;
	}
	vars.push_back(var);
}

std::vector<std::string> VarDeclMetadata::getVars() { return vars; }