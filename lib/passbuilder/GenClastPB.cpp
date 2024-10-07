#include <pesto/Pesto.hpp>

using namespace pesto;

GenClastPB::GenClastPB() {}

GenClastPB::~GenClastPB() {}

std::unique_ptr<pesto::IPestoPass> GenClastPB::build(PMBuilder &builder,
													 const json &j) {
	return std::make_unique<GenClastPass>();
}
