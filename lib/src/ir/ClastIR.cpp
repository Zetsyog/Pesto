#include <pesto/ir/ClastIR.hpp>

const std::string pesto::ir::ClastIR::ID = "clast";

pesto::ir::ClastIR::~ClastIR() {
	if (root_) {
		cloog_clast_free(root_);
	}
	if (cloogProgram_) {
		cloog_program_free(cloogProgram_);
	}
	if (cloogOptions_) {
		cloog_options_free(cloogOptions_);
	}
	if (cloogState_) {
		cloog_state_free(cloogState_);
	}
}