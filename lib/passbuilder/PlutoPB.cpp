#include <pesto/Pesto.hpp>

using namespace pesto;

#define PESTO_BIND_PLUTO_OPT(json, pluto_opt, name)                            \
	if (json.contains(#name)) {                                                \
		json.at(#name).get_to(pluto_opt->name);                                \
	}

PlutoPB::PlutoPB() {}

PlutoPB::~PlutoPB() {}

std::unique_ptr<pesto::IPestoPass> PlutoPB::build(PMBuilder &builder,
												  const json &j) {
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
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), tile);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), parallel);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), prevector);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), intratileopt);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), unrolljam);

	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), silent);
	PESTO_BIND_PLUTO_OPT(pluto_opt, pass->getOptions(), debug);

	std::cerr << "INTRATILEOPT: " << pass->getOptions()->intratileopt
			  << std::endl;

	return pass;
}