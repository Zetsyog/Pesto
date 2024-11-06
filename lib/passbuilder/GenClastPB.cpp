#include <pesto/Pesto.hpp>

using namespace pesto::builder;

GenClastPB::GenClastPB() {}

GenClastPB::~GenClastPB() {}

std::unique_ptr<pesto::pass::IPass>
GenClastPB::build(PassManagerBuilder &builder, const json &j) {
	(void)builder;
	(void)j;
	return std::make_unique<pesto::pass::GenClastPass>();
}
