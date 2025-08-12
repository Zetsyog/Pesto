#include <pesto/pass/TpzTilingPass.hpp>
#include <pesto/passbuilder/TpzTilingPB.hpp>

using namespace pesto::builder;

TpzTilingPB::TpzTilingPB() {}

TpzTilingPB::~TpzTilingPB() {}

std::unique_ptr<pesto::pass::IPass> TpzTilingPB::build(PassManagerBuilder &builder, const json &j) {
	using pesto::pass::TpzTilingPass;

	(void)builder;

	std::unique_ptr<TpzTilingPass> pass = std::make_unique<TpzTilingPass>();

	return pass;
}
