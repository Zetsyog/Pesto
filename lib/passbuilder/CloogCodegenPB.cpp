#include <pesto/macros.h>
#include <pesto/pass/CodegenPasses.hpp>
#include <pesto/passbuilder/CloogCodegenPB.hpp>

using namespace pesto::builder;

CloogCodegenPB::CloogCodegenPB() {}

CloogCodegenPB::~CloogCodegenPB() {}

std::unique_ptr<pesto::pass::IPass> CloogCodegenPB::build(PassManagerBuilder &builder, const json &j) {
	(void)builder;
	(void)j;

	auto pass = std::make_unique<pass::CloogCodegenPass>();

	std::unique_ptr<pass::GenClastPass> clastPass =
		std::unique_ptr<pass::GenClastPass>(static_cast<pass::GenClastPass *>(
			builder.getPassBuilder(PESTO_UUID_PASS_CLAST_GEN)->build(builder, j).release()));
	if (clastPass) {
		pass->setGenClastPass(std::move(clastPass));
	} else {
		std::cerr << "Error: "
				  << "Could not create clast pass" << std::endl;
		return nullptr;
	}

	return pass;
}
