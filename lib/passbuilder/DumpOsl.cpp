#include <pesto/pass/OslPass.hpp>
#include <pesto/passbuilder/DumpOsl.hpp>

using namespace pesto::builder;

OslDumpPB::OslDumpPB() {}

OslDumpPB::~OslDumpPB() {}

std::unique_ptr<pesto::pass::IPass> OslDumpPB::build(PassManagerBuilder &builder, const json &j) {
	(void)builder;
	(void)j;
	return std::make_unique<pesto::pass::DumpOslPass>("dump.osl");
}