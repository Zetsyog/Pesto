#include <pesto/Pesto.hpp>

using namespace pesto;

GenClastPB::GenClastPB() {}

GenClastPB::~GenClastPB() {}

std::string GenClastPB::getUUID() { return PESTO_UUID_PASS_CLAST_GEN; }

std::unique_ptr<pesto::IPestoPass> GenClastPB::build(PMBuilder &builder,const json &j) {
	return std::make_unique<GenClastPass>();
}
