#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <string>

#include <isl/aff.h>
#include <isl/set.h>

#include <pesto/clast/ParamTilingPass.hpp>
#include <pesto/clast/clast_support.h>
#include <pesto/codegen/ParamTiling.hpp>

#define INDENT_TABS(os, n)                                                                                             \
	for (int i = 0; i < n; ++i)                                                                                        \
		os << "\t";

using namespace pesto::ptile::codegen;

ParamTilingCodegen::ParamTilingCodegen(StmtGroup &group, TilingData &data, int nestId, __isl_take isl_set *innerDomain)
	: group(group)
	, data(data)
	, tilingType(ParamTilingType::ALGEBRAIC)
	, outterFor(*data.outter_for)
	, innerFor(*data.inner_for)
	, tilingDepth(data.tiling_depth)
	, innerDomain(nullptr)
	, boundComputeMethod(BoundComputeMethod::FILL_LAST)
	, endline(std::string("\\\n"))
	, boundCorrectionStrategy(BoundCorrectionStrategy::NO_CORRECTION)
	, volumeCheckStrategy(VolumeCheckStrategy::NO_CHECK)
	, volumeCheckThreshold("")
	, nameHelper(tilingDepth, outterFor.iterator, innerFor.iterator)
	, trahrheNameHelper(tilingDepth, outterFor.iterator, innerFor.iterator) {
	this->innerDomain = isl_set_copy(innerDomain);
	isl_set_free(innerDomain);
	this->nameHelper.setPrefix("g" + std::to_string(group.id));
	this->trahrheNameHelper.setPrefix("n" + std::to_string(nestId) + this->nameHelper.getSeparator() + "g" +
									  std::to_string(group.id));
	this->volumeCheckThreshold = this->nameHelper.divName() + " * 10";
}

ParamTilingCodegen::~ParamTilingCodegen() {
	isl_set_free(this->innerDomain);
	this->innerDomain = nullptr;
}

void ParamTilingCodegen::setTilingType(pesto::ParamTilingType type) { this->tilingType = type; }

void ParamTilingCodegen::setBoundComputeMethod(BoundComputeMethod method) { this->boundComputeMethod = method; }
void ParamTilingCodegen::setBoundCorrectionStrategy(BoundCorrectionStrategy strategy) {
	this->boundCorrectionStrategy = strategy;
}
void ParamTilingCodegen::setVolumeCheckStrategy(VolumeCheckStrategy strategy) { this->volumeCheckStrategy = strategy; }

void ParamTilingCodegen::genStmt(std::ostream &os, StmtType type) {
	switch (type) {
	case StmtType::HEAD:
		this->genHeaderStmt(os);
		break;
	case StmtType::BODY:
		this->genBodyStmt(os);
		break;
	default:
		std::cerr << "[param-tiling] Unknown statement type" << std::endl;
	}
}

void ParamTilingCodegen::genHeaderStmt(std::ostream &os) {
	switch (this->tilingType) {
	case pesto::ParamTilingType::RECTANGULAR:
		this->genHeaderStmtRectangular(os);
		break;
	case pesto::ParamTilingType::ALGEBRAIC:
		this->genHeaderStmtAlgebraic(os);
		break;
	case pesto::ParamTilingType::SCHEDULED:
		this->genHeaderStmtScheduled(os);
		break;
	default:
		std::cerr << "[param-tiling] Unknown tiling type" << std::endl;
	}
}

void ParamTilingCodegen::genHeaderStmtRectangular(std::ostream &os) {
	os << this->nameHelper.outterForUBName() << " = ceild(" << this->data.innerForUBExpr;
	if (this->data.innerForLBExpr.length() > 0) {
		os << " - (" << this->data.innerForLBExpr << ")";
	}
	os << ", " << this->nameHelper.divName() << ")";
}

void ParamTilingCodegen::genHeaderStmtAlgebraic(std::ostream &os) {
	isl_size npar = isl_set_dim(this->innerDomain, isl_dim_param);
	os << this->nameHelper.outterForUBName() << " = " << this->nameHelper.divName() << " - 1;" << this->endline;
	os << this->nameHelper.pcMaxName() << " = " << this->trahrheNameHelper.ehrhartFnName() << "(";
	for (isl_size i = 0; i < npar; i += 1) {
		if (i != 0) {
			os << ", ";
		}
		os << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	for (std::size_t i = 0; i < this->tilingDepth; i += 1) {
		auto &data = this->group.tilingData[i];
		NameHelper helper = NameHelper(data.tiling_depth, data.outter_for->iterator, data.inner_for->iterator);
		helper.setPrefix(this->nameHelper.getPrefix());
		helper.setSeparator(this->nameHelper.getSeparator());
		os << ", " << helper.innerForLBName();
		os << ", " << helper.innerForUBName();
	}
	os << ");" << this->endline;
	os << this->nameHelper.targetVolName() << " = ";
	if (this->boundComputeMethod == BoundComputeMethod::FILL_FIRST) {
		os << "ceild(" << this->nameHelper.pcMaxName() << ", " << this->nameHelper.divName() << ")";
	}
	if (this->boundComputeMethod == BoundComputeMethod::FILL_LAST) {
		os << this->nameHelper.pcMaxName() << " / " << this->nameHelper.divName();
	}
	if (this->volumeCheckStrategy != VolumeCheckStrategy::NO_CHECK) {
		os << ";" << this->endline;
		// TODO: volume treshold should be a parameter
		os << "if(" << this->nameHelper.targetVolName() << " <= " << this->volumeCheckThreshold << ") {"
		   << this->endline;

		if (this->volumeCheckStrategy == VolumeCheckStrategy::CHECK_AND_WARN ||
			this->volumeCheckStrategy == VolumeCheckStrategy::CHECK_AND_DIE) {
			os << "\tfprintf(stderr, \"Warning: computed volume is too small (" << this->nameHelper.targetVolName()
			   << " <= " << this->volumeCheckThreshold
			   << ").\\nTry with smaller divider or bigger parameters to increase tile "
				  "volume.\\n\");"
			   << this->endline;
			if (this->volumeCheckStrategy == VolumeCheckStrategy::CHECK_AND_DIE) {
				os << "\texit(1);" << this->endline;
			}
		} else if (this->volumeCheckStrategy == VolumeCheckStrategy::NO_TILE_UNTIL_THRESHOLD) {
			os << "\t" << this->nameHelper.targetVolName() << " = " << this->nameHelper.pcMaxName() << ";"
			   << this->endline;
			os << "\t" << this->nameHelper.outterForUBName() << " = 0;" << this->endline;
		} else if (this->volumeCheckStrategy == VolumeCheckStrategy::DYNAMIC_TILE_UNTIL_THRESHOLD) {
			std::string dynDividerName = "dynamicDivider";
			std::string minDividerName = "minDivider";
			os << "\tunsigned long " << dynDividerName << " = 1;" << this->endline;
			os << "\tunsigned long " << minDividerName << " = max(1, floord(" << this->nameHelper.pcMaxName() << ", "
			   << this->volumeCheckThreshold << "));" << this->endline;
			os << "\tif(" << minDividerName << " < " << this->nameHelper.divName() << ") {" << this->endline;
			os << "\t\t" << dynDividerName << " = " << minDividerName << ";" << this->endline;
			os << "\t} else {" << this->endline;
			os << "\t\t" << dynDividerName << " = floord(" << minDividerName << ", " << this->nameHelper.divName()
			   << ") * " << this->nameHelper.divName() << ";" << this->endline;
			os << "\t}" << this->endline;
			os << "\t" << this->nameHelper.targetVolName() << " = " << this->nameHelper.pcMaxName() << " / "
			   << dynDividerName << ";" << this->endline;
			os << "\t" << this->nameHelper.outterForUBName() << " = " << dynDividerName << " - 1;" << this->endline;
		}
		os << "}";
	}
}

void ParamTilingCodegen::genBodyStmt(std::ostream &os) {
	switch (this->tilingType) {
	case pesto::ParamTilingType::RECTANGULAR:
		this->genBodyStmtRectangular(os);
		break;
	case pesto::ParamTilingType::ALGEBRAIC:
		this->genBodyStmtAlgebraic(os);
		break;
	case pesto::ParamTilingType::SCHEDULED:
		this->genBodyStmtScheduled(os);
		break;
	default:
		std::cerr << "[param-tiling] Unknown tiling type" << std::endl;
	}
}

void ParamTilingCodegen::genBodyStmtRectangular(std::ostream &os) {
	os << this->nameHelper.innerForLBName() << " = " << this->nameHelper.divName() << " * " << this->outterFor.iterator;
	os << "+" << this->data.innerForLBExpr << ";" << this->endline;

	os << this->nameHelper.innerForUBName() << " = " << this->nameHelper.divName() << " * " << this->outterFor.iterator
	   << " + " << this->nameHelper.divName() << " - 1";
	os << "+" << this->data.innerForLBExpr;
}

void ParamTilingCodegen::genBodyStmtAlgebraic(std::ostream &os) {
	isl_size npar = isl_set_dim(this->innerDomain, isl_dim_param);

	// inner for lower bound
	os << this->nameHelper.innerForLBName() << " = ";
	os << this->trahrheNameHelper.trahrheFnName() << "(max(1, (" << this->outterFor.iterator << ") * "
	   << this->nameHelper.targetVolName() << ")";
	for (isl_size i = 0; i < npar; i += 1) {
		os << ", " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}

	for (std::size_t i = 0; i < this->tilingDepth; i += 1) {
		auto &data = this->group.tilingData[i];
		NameHelper helper = NameHelper(data.tiling_depth, data.outter_for->iterator, data.inner_for->iterator);
		helper.setPrefix(this->nameHelper.getPrefix());
		helper.setSeparator(this->nameHelper.getSeparator());
		os << ", " << helper.innerForLBName();
		os << ", " << helper.innerForUBName();
	}
	os << ")";
	if (this->boundComputeMethod == BoundComputeMethod::FILL_FIRST) {
		os << " + !!" << this->outterFor.iterator;
	}
	os << ";" << this->endline;
	// inner for upper bound
	os << this->nameHelper.innerForUBName() << " = ";
	os << this->trahrheNameHelper.trahrheFnName() << "(min(" << this->nameHelper.pcMaxName() << ", ("
	   << this->outterFor.iterator << " + 1) * " << this->nameHelper.targetVolName() << ")";
	for (isl_size i = 0; i < npar; i += 1) {
		os << ", " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	for (std::size_t i = 0; i < this->tilingDepth; i += 1) {
		auto &data = this->group.tilingData[i];
		NameHelper helper = NameHelper(data.tiling_depth, data.outter_for->iterator, data.inner_for->iterator);
		helper.setPrefix(this->nameHelper.getPrefix());
		helper.setSeparator(this->nameHelper.getSeparator());
		os << ", " << helper.innerForLBName();
		os << ", " << helper.innerForUBName();
	}
	os << ")";
	if (this->boundComputeMethod == BoundComputeMethod::FILL_LAST) {
		os << " - 1";
	}
	// last bound adjustment
	if (this->boundComputeMethod == BoundComputeMethod::FILL_LAST) {
		os << ";" << this->endline;
		os << "if(" << this->outterFor.iterator << " == " << this->nameHelper.outterForUBName() << ") {"
		   << this->endline;
		os << "\t" << this->nameHelper.innerForUBName() << " = " << this->data.innerForUBExpr << ";" << this->endline;
		os << "}";
	}

	if (this->boundCorrectionStrategy != BoundCorrectionStrategy::NO_CORRECTION) {
		os << ";" << this->endline;
		os << "if(" << this->nameHelper.innerForLBName() << " > " << this->nameHelper.innerForUBName() << ") {"
		   << this->endline;
		if (this->boundCorrectionStrategy == BoundCorrectionStrategy::SHOUT_AND_CONTINUE ||
			this->boundCorrectionStrategy == BoundCorrectionStrategy::SHOUT_AND_DIE) {
			os << "\tfprintf(stderr, \"Error: computed bounds are incorrects (" << this->nameHelper.innerForLBName()
			   << " > " << this->nameHelper.innerForUBName()
			   << ").\\nTry with smaller divider or bigger parameters to increase tile volume.\\n\");" << this->endline;
			if (this->boundCorrectionStrategy == BoundCorrectionStrategy::SHOUT_AND_DIE) {
				os << "\texit(1);" << this->endline;
			}
		} else if (this->boundCorrectionStrategy == BoundCorrectionStrategy::CORRECT) {
			os << "\t" << this->nameHelper.innerForUBName() << " = " << this->nameHelper.innerForLBName() << ";"
			   << this->endline;
		}
		os << "}";
	}
}

void ParamTilingCodegen::setEndline(std::string endline) { this->endline = endline; }

void ParamTilingCodegen::setVolumeCheckThreshold(std::string threshold) {
	if (threshold.empty()) {
		std::cerr << "[param-tiling] Warning: volume check threshold is empty. Default to "
				  << this->volumeCheckThreshold << std::endl;
		return;
	}
	this->volumeCheckThreshold = threshold;
}

void ParamTilingCodegen::genHeaderStmtScheduled(std::ostream &os) {
	std::string tileArrayName = this->trahrheNameHelper.tileArrayName();
	std::string scheduleName = this->trahrheNameHelper.scheduleName();
	isl_size npar = isl_set_dim(this->innerDomain, isl_dim_param);

	if (this->tilingDepth == 0) {
		os << "SCHEDULE_ALLOC(" << tileArrayName << ", " << scheduleName << ");" << this->endline;
		os << "SCHEDULE_COMPUTE(" << this->trahrheNameHelper.getPrefix() << ", " << tileArrayName << ", "
		   << scheduleName;
		for (isl_size i = 0; i < npar; i += 1) {
			os << ", " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
		}
		os << ");" << this->endline;

		os << this->nameHelper.outterForUBName() << " = " << scheduleName << ".len - 1;" << this->endline;
	}
	if (this->tilingDepth == 1) {
		os << this->nameHelper.outterForUBName() << " = " << scheduleName << ".fronts["
		   << this->group.tilingData[0].outter_for->iterator << "].len - 1;" << this->endline;
	}
}

void ParamTilingCodegen::genBodyStmtScheduled(std::ostream &os) {
	if (this->tilingDepth == 0) {
		return;
	}
	if (this->tilingDepth > 1) {
		std::cerr << "[param-tiling] Warning: scheduled tiling depth is > 1. This is really weird..." << std::endl;
		return;
	}

	std::string tileArrayName = this->trahrheNameHelper.tileArrayName();
	std::string scheduleName = this->trahrheNameHelper.scheduleName();

	auto &outterData = this->group.tilingData[0];
	NameHelper outterHelper =
		NameHelper(outterData.tiling_depth, outterData.outter_for->iterator, outterData.inner_for->iterator);
	outterHelper.setPrefix(this->nameHelper.getPrefix());
	outterHelper.setSeparator(this->nameHelper.getSeparator());
	std::string scheduleAccessName =
		scheduleName + ".fronts[" + outterData.outter_for->iterator + "].tiles[" + this->outterFor.iterator + "]";

	os << outterHelper.innerForLBName() << " = " << scheduleAccessName << "->lb0;" << this->endline;
	os << outterHelper.innerForUBName() << " = " << scheduleAccessName << "->ub0;" << this->endline;
	os << this->nameHelper.innerForLBName() << " = " << scheduleAccessName << "->lb1;" << this->endline;
	os << this->nameHelper.innerForUBName() << " = " << scheduleAccessName << "->ub1;" << this->endline;
}

void ParamTilingCodegen::genScheduleHeaderFile(std::ostream &os) {
	// sanity check
	if (this->tilingDepth != 0) {
		std::cerr << "[param-tiling] Error: schedule header file can only be generated for tiling depth 0" << std::endl;
		return;
	}
	if (this->group.tilingData.size() < 2) {
		std::cerr << "[param-tiling] Error: schedule header file can only be generated for tiling depth 0 and 1"
				  << std::endl;
		return;
	}
	if (this->tilingType != ParamTilingType::SCHEDULED) {
		std::cerr << "[param-tiling] Error: schedule header file can only be generated for scheduled tiling"
				  << std::endl;
		return;
	}
	// done sanity check
	// generate the header file

	std::string headerMacro = "PESTO_RUNTIME_SCHEDULE_" + this->trahrheNameHelper.getPrefix() + "_H";
	std::string varType = "long int";
	isl_size npar = isl_set_dim(this->innerDomain, isl_dim_param);

	TilingData &t1Data = this->group.tilingData[1];
	NameHelper t1NameHelper = NameHelper(1, t1Data.outter_for->iterator, t1Data.inner_for->iterator);
	t1NameHelper.setPrefix("");
	t1NameHelper.setSeparator(this->nameHelper.getSeparator());
	NameHelper t1TrahrheHelper = NameHelper(1, t1Data.outter_for->iterator, t1Data.inner_for->iterator);
	t1TrahrheHelper.setPrefix(this->trahrheNameHelper.getPrefix());
	t1TrahrheHelper.setSeparator(this->trahrheNameHelper.getSeparator());

	// multi include guard
	os << "#ifndef " << headerMacro << std::endl;
	os << "#define " << headerMacro << std::endl;

	// copy template schedule.h
	std::ifstream scheduleTemplate(_PESTO_TEMPLATE_SCHEDULE_PATH, std::ios::in);
	if (!scheduleTemplate.is_open()) {
		std::cerr << "[param-tiling] Error: cannot open schedule template file" << std::endl;
		return;
	}
	os << scheduleTemplate.rdbuf() << std::endl;
	scheduleTemplate.close();

	// init tiles function
	os << "PESTO_RUNTIME_INIT_TILE_PROTO(" << this->trahrheNameHelper.getPrefix();
	for (isl_size i = 0; i < npar; i += 1) {
		os << ", " << varType << " " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	os << ") {" << this->endline;

	// name helper is not required here, we are declaring local scope variables
	// but required for the trahrhe functions calls
	INDENT_TABS(os, 1);
	os << varType << " " << this->nameHelper.pcMaxName() << " = " << this->trahrheNameHelper.ehrhartFnName() << "(";
	for (isl_size i = 0; i < npar; i += 1) {
		if (i != 0) {
			os << ", ";
		}
		os << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	os << ");" << this->endline;

	INDENT_TABS(os, 1);
	os << varType << " " << this->nameHelper.targetVolName() << " = " << this->nameHelper.pcMaxName() << " / "
	   << this->nameHelper.divName() << ";" << this->endline;

	INDENT_TABS(os, 1);
	os << varType << " " << t1NameHelper.pcMaxName() << ", " << t1NameHelper.targetVolName() << ";" << this->endline;

	INDENT_TABS(os, 1);
	os << varType << " t1;" << this->endline;

	INDENT_TABS(os, 1);
	os << varType << " " << this->nameHelper.innerForLBName() << ", " << this->nameHelper.innerForUBName() << ";"
	   << this->endline;

	os << "#pragma omp parallel for firstprivate(" << this->nameHelper.pcMaxName() << ", "
	   << this->nameHelper.targetVolName() << ") " //
	   << "private(t1, " << t1NameHelper.pcMaxName() << ", " << t1NameHelper.targetVolName() << ", "
	   << this->nameHelper.innerForLBName() << ", " << this->nameHelper.innerForUBName() << ")" << this->endline;
	INDENT_TABS(os, 1);
	os << "for(" << varType << " t0 = 0; t0 < " << this->nameHelper.divName() << "; ++t0) {" << this->endline;
	INDENT_TABS(os, 2);
	os << varType << " " << this->nameHelper.innerForLBName() << " = " << this->trahrheNameHelper.trahrheFnName()
	   << "(max(1, t0 * " << this->nameHelper.targetVolName() << ")";
	for (isl_size i = 0; i < npar; i += 1) {
		os << ", " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	os << ");" << this->endline;

	INDENT_TABS(os, 2);
	os << varType << " " << this->nameHelper.innerForUBName() << " = " << this->trahrheNameHelper.trahrheFnName()
	   << "(min(" << this->nameHelper.pcMaxName() << ", (t0 + 1) * " << this->nameHelper.targetVolName() << ")";
	for (isl_size i = 0; i < npar; i += 1) {
		os << ", " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	os << ") - 1;" << this->endline;

	INDENT_TABS(os, 2);
	os << "if(t0 == " << this->nameHelper.divName() << " - 1) {" << this->endline;
	INDENT_TABS(os, 3);
	os << this->nameHelper.innerForUBName() << " = " << this->data.innerForUBExpr << ";" << this->endline;
	INDENT_TABS(os, 2);
	os << "}" << this->endline;

	INDENT_TABS(os, 2);
	os << t1NameHelper.pcMaxName() << " = " << t1TrahrheHelper.ehrhartFnName() << "(";
	for (isl_size i = 0; i < npar; i += 1) {
		if (i != 0) {
			os << ", ";
		}
		os << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	os << ", " << this->nameHelper.innerForLBName() << ", " << this->nameHelper.innerForUBName() << ");"
	   << this->endline;

	INDENT_TABS(os, 2);
	os << t1NameHelper.targetVolName() << " = " << t1NameHelper.pcMaxName() << " / " << t1NameHelper.divName() << ";"
	   << this->endline;

	INDENT_TABS(os, 2);
	os << "for(t1 = 0; t1 < " << t1NameHelper.divName() << "; ++t1) {" << this->endline;

	INDENT_TABS(os, 3);
	os << "tileArray[t0][t1].t0 = t0;" << this->endline;
	INDENT_TABS(os, 3);
	os << "tileArray[t0][t1].t1 = t1;" << this->endline;
	INDENT_TABS(os, 3);
	os << "tileArray[t0][t1].lb0 = " << this->nameHelper.innerForLBName() << ";" << this->endline;
	INDENT_TABS(os, 3);
	os << "tileArray[t0][t1].ub0 = " << this->nameHelper.innerForUBName() << ";" << this->endline;
	INDENT_TABS(os, 3);
	os << "tileArray[t0][t1].lb1 = " << t1TrahrheHelper.trahrheFnName() << "(max(1, t1 * "
	   << t1NameHelper.targetVolName() << ")";
	for (isl_size i = 0; i < npar; i += 1) {
		os << ", " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	os << ", " << this->nameHelper.innerForLBName() << ", " << this->nameHelper.innerForUBName() << ");"
	   << this->endline;
	INDENT_TABS(os, 3);
	os << "tileArray[t0][t1].ub1 = " << t1TrahrheHelper.trahrheFnName() << "(min(" << t1NameHelper.pcMaxName()
	   << ", (t1 + 1) * " << t1NameHelper.targetVolName() << ")";
	for (isl_size i = 0; i < npar; i += 1) {
		os << ", " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	os << ", " << this->nameHelper.innerForLBName() << ", " << this->nameHelper.innerForUBName() << ") - 1;"
	   << this->endline;
	INDENT_TABS(os, 3);
	os << "tileArray[t0][t1].marked = 0;" << this->endline;
	INDENT_TABS(os, 3);
	os << "if(t1 == " << t1NameHelper.divName() << " - 1) {" << this->endline;
	INDENT_TABS(os, 4);
	os << "tileArray[t0][t1].ub1 = " << this->group.tilingData[1].innerForUBExpr << ";" << this->endline;
	INDENT_TABS(os, 3);
	os << "}" << this->endline;

	INDENT_TABS(os, 2);
	os << "}" << this->endline;

	INDENT_TABS(os, 1);
	os << "}" << this->endline;

	os << "}" << this->endline;

	os << "#endif // " << headerMacro << this->endline;
}

using pesto::ptile::NameHelper;

NameHelper::NameHelper(int depth, std::string outterForIt, std::string innerForIt)
	: depth(depth)
	, sep("_")
	, prefix("")
	, outterForIt(outterForIt)
	, innerForIt(innerForIt) //
{}

NameHelper::~NameHelper() {}

std::string &NameHelper::getSeparator() { return this->sep; }
std::string &NameHelper::getPrefix() { return this->prefix; }

void NameHelper::setSeparator(std::string sep) { this->sep = sep; }
void NameHelper::setPrefix(std::string prefix) { this->prefix = prefix; }

std::string NameHelper::outterForUBName() { return this->prefix + this->sep + "ub" + this->outterForIt; }
std::string NameHelper::innerForLBName() { return this->prefix + this->sep + "lb" + this->innerForIt; }
std::string NameHelper::innerForUBName() { return this->prefix + this->sep + "ub" + this->innerForIt; }
std::string NameHelper::divName() { return "DIV" + std::to_string(this->depth); }
std::string NameHelper::targetVolName() {
	return this->prefix + this->sep + "TARGET" + this->sep + "VOL" + this->sep + "L" + std::to_string(this->depth);
}
std::string NameHelper::pcMaxName() { return this->prefix + this->sep + this->innerForIt + this->sep + "pcmax"; }
std::string NameHelper::ehrhartFnName() {
	std::string itName = this->prefix + this->sep + this->innerForIt;
	return itName + this->sep + "Ehrhart";
}
std::string NameHelper::trahrheFnName() {
	std::string itName = this->prefix + this->sep + this->innerForIt;
	return itName + this->sep + "trahrhe" + this->sep + itName;
}
std::string NameHelper::trahrheIteratorName() { return this->prefix + this->sep + this->innerForIt; }

std::string NameHelper::scheduleName() { return this->prefix + this->sep + "schedule"; }
std::string NameHelper::tileArrayName() { return this->prefix + this->sep + "tiles"; }