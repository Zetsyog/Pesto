#include <pesto/Pesto.hpp>

using namespace pesto;

CloogCodegenPB::CloogCodegenPB() {}

CloogCodegenPB::~CloogCodegenPB() {}

std::string CloogCodegenPB::getUUID() { return PESTO_UUID_PASS_GEN_CLOOG; }

std::unique_ptr<pesto::IPolyhedralPass> CloogCodegenPB::build(const json &j) {
	return std::make_unique<CloogCodegenPass>(".pesto.gen");
}
