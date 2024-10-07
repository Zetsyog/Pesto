#include <pesto/Pesto.hpp>

using namespace pesto;

SplitNestPB::SplitNestPB() {}

SplitNestPB::~SplitNestPB() {}

std::string SplitNestPB::getUUID() {
	return PESTO_UUID_PASS_SPLIT_NESTED_LOOPS;
}

std::unique_ptr<pesto::IPestoPass> SplitNestPB::build(PMBuilder &builder,
													  const json &j) {
	return std::make_unique<SplitNestedLoopsPass>();
}