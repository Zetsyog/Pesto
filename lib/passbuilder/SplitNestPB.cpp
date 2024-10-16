#include <pesto/Pesto.hpp>

using namespace pesto::builder;

SplitNestPB::SplitNestPB() {}

SplitNestPB::~SplitNestPB() {}

std::unique_ptr<pesto::pass::IPass>
SplitNestPB::build(PassManagerBuilder &builder, const json &j) {
	return std::make_unique<pesto::pass::SplitNestedLoopsPass>();
}