#include <pesto/pass/SplitTilingPass.hpp>
#include <pesto/passbuilder/SplitTilingPB.hpp>

using namespace pesto::builder;

SplitTilingPB::SplitTilingPB() {}

SplitTilingPB::~SplitTilingPB() {}

std::unique_ptr<pesto::pass::IPass> SplitTilingPB::build(PassManagerBuilder &builder, const json &j) {
	using pesto::pass::SplitTilingPass;

	(void)builder;

	std::unique_ptr<SplitTilingPass> pass = std::make_unique<SplitTilingPass>();

	return pass;
}
