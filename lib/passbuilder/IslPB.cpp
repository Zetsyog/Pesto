#include <pesto/pass/IslPass.hpp>
#include <pesto/passbuilder/IslPB.hpp>

using namespace pesto::builder;

IslPB::IslPB() {}

IslPB::~IslPB() {}

std::unique_ptr<pesto::pass::IPass> IslPB::build(PassManagerBuilder &builder, const json &j) {
	using pesto::pass::IslPass;

	(void)builder;

	std::unique_ptr<IslPass> pass = std::make_unique<IslPass>();

	return pass;
}
