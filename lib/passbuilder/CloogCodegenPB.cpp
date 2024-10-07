#include <pesto/Pesto.hpp>

using namespace pesto;

CloogCodegenPB::CloogCodegenPB() {}

CloogCodegenPB::~CloogCodegenPB() {}

std::string CloogCodegenPB::getUUID() { return PESTO_UUID_PASS_GEN_CLOOG; }

std::unique_ptr<pesto::IPestoPass> CloogCodegenPB::build(PMBuilder &builder,const json &j) {
	return std::make_unique<CloogCodegenPass>(".pesto.gen");
}
