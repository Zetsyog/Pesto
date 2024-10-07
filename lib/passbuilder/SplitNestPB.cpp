#include <pesto/Pesto.hpp>

using namespace pesto;

SplitNestPB::SplitNestPB() {}

SplitNestPB::~SplitNestPB() {}

std::unique_ptr<pesto::IPestoPass> SplitNestPB::build(PMBuilder &builder,
													  const json &j) {
	return std::make_unique<SplitNestedLoopsPass>();
}