#include <pesto/Pesto.hpp>

using namespace pesto;

GenClastPB::GenClastPB() {}

GenClastPB::~GenClastPB() {}

std::string GenClastPB::getUUID() { return PESTO_UUID_PASS_CLAST_GEN; }

std::unique_ptr<pesto::IPolyhedralPass> GenClastPB::build(const json &j) {
	return std::make_unique<GenClastPass>();
}
