#include <pesto/Pesto.hpp>

using namespace pesto;

CloogCodegenPB::CloogCodegenPB() {}

CloogCodegenPB::~CloogCodegenPB() {}


std::unique_ptr<pesto::IPestoPass> CloogCodegenPB::build(PMBuilder &builder,const json &j) {
	return std::make_unique<CloogCodegenPass>(".pesto.gen");
}
