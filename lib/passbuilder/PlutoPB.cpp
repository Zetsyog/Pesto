#include <pesto/Pesto.hpp>

using namespace pesto;

PlutoPB::PlutoPB() {}

PlutoPB::~PlutoPB() {}

std::string PlutoPB::getUUID() { return PESTO_UUID_PASS_PLUTO_GENERIC; }

std::unique_ptr<pesto::IPolyhedralPass> PlutoPB::build(const json &j) {
	// TODO Parse the JSON object and build the pass
	auto pass = std::make_unique<PlutoPass>();
	if (!j.contains("options")) {
		return pass;
	}

	auto options = j["options"];
	if (!options.contains("pluto")) {
		return pass;
	}

	auto pluto_opt = options["pluto"];

	// TODO: parse more options
	if (pluto_opt.contains("tile")) {
		pluto_opt.at("tile").get_to(pass->getOptions()->tile);
		std::cerr << "Tile: " << pass->getOptions()->tile << std::endl;
	}
	if (pluto_opt.contains("parallel")) {
		pluto_opt.at("parallel").get_to(pass->getOptions()->parallel);
	}
	if (pluto_opt.contains("prevector")) {
		pluto_opt.at("prevector").get_to(pass->getOptions()->prevector);
	}
	if (pluto_opt.contains("silent")) {
		pluto_opt.at("silent").get_to(pass->getOptions()->silent);
	}
	if (pluto_opt.contains("debug")) {
		pluto_opt.at("debug").get_to(pass->getOptions()->debug);
	}
	return pass;
}