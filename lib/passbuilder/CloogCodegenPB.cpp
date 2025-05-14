#include <pesto/CodegenPasses.hpp>
#include <pesto/passbuilder/CloogCodegenPB.hpp>

using namespace pesto::builder;

CloogCodegenPB::CloogCodegenPB() {}

CloogCodegenPB::~CloogCodegenPB() {}

std::unique_ptr<pesto::pass::IPass> CloogCodegenPB::build(PassManagerBuilder &builder, const json &j) {
	(void)builder;
	(void)j;

	return std::make_unique<pesto::pass::CloogCodegenPass>();
}
