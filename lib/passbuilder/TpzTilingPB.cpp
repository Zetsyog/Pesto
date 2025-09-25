#include <pesto/pass/TpzTilingPass.hpp>
#include <pesto/passbuilder/TpzTilingPB.hpp>

using namespace pesto::builder;

TpzTilingPB::TpzTilingPB() {}

TpzTilingPB::~TpzTilingPB() {}

std::unique_ptr<pesto::pass::IPass> TpzTilingPB::build(PassManagerBuilder &builder, const json &j) {
	using pesto::pass::TpzTilingPass;

	(void)builder;

	std::unique_ptr<TpzTilingPass> pass = std::make_unique<TpzTilingPass>();

	for (auto &[key, val] : j.items()) {
		// ignore uuid
		if (key == "uuid") {
			continue;
		} else if (key == "useMinVolume") {
			if (!val.is_boolean()) {
				std::cerr << "Warning: " << key << " must be a boolean. Ignoring." << std::endl;
				continue;
			}
			pass->setUseMinVolume(val.get<bool>());

		} else {
			std::cerr << "Warning: Unknown option " << key << ". Ignoring." << std::endl;
		}
	}

	return pass;
}
