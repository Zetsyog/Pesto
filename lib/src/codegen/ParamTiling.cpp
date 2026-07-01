/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <algorithm>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <string>
#include <vector>

#include <isl/aff.h>
#include <isl/set.h>

#include <pesto/clast_support.h>
#include <pesto/codegen/ParamTiling.hpp>
#include <pesto/config.h>
#include <pesto/pass/clast/ParamTilingPass.hpp>

#define INDENT_TABS(os, n)                                                                                             \
	for (int i = 0; i < n; ++i)                                                                                        \
		os << "\t";

using namespace pesto::ptile::codegen;

ParamTilingCodegen::ParamTilingCodegen(StmtGroup &group, TilingData &data, int nestId, __isl_take isl_set *innerDomain)
	: group(group)
	, data(data)
	, tilingType(ParamTilingType::ALGEBRAIC)
	, outerFor(*data.outer_for)
	, tilingDepth(data.tiling_depth)
	, innerDomain(nullptr)
	, boundComputeMethod(BoundComputeMethod::FILL_LAST)
	, endline(std::string("\\\n"))
	, boundCorrectionStrategy(BoundCorrectionStrategy::NO_CORRECTION)
	, volumeCheckStrategy(VolumeCheckStrategy::NO_CHECK)
	, volumeCheckThreshold("")
	, nameHelper(*data.nameHelper)
	, trahrheNameHelper(*data.trahrheNameHelper) {
	this->innerDomain = isl_set_copy(innerDomain);
	isl_set_free(innerDomain);
	this->volumeCheckThreshold = this->nameHelper.divName() + " * 1000";
	this->logger = pesto::log::get_logger("param-tiling-codegen");
}

ParamTilingCodegen::~ParamTilingCodegen() {
	isl_set_free(this->innerDomain);
	this->innerDomain = nullptr;
}

void ParamTilingCodegen::setTilingType(pesto::ParamTilingType type) {
	this->tilingType = type;
}

void ParamTilingCodegen::setBoundComputeMethod(BoundComputeMethod method) {
	this->boundComputeMethod = method;
}
void ParamTilingCodegen::setBoundCorrectionStrategy(BoundCorrectionStrategy strategy) {
	this->boundCorrectionStrategy = strategy;
}
void ParamTilingCodegen::setVolumeCheckStrategy(VolumeCheckStrategy strategy) {
	this->volumeCheckStrategy = strategy;
}

void ParamTilingCodegen::genStmt(std::ostream &os, StmtType type) {
	switch (type) {
	case StmtType::HEAD:
		this->genHeaderStmt(os);
		break;
	case StmtType::BODY:
		this->genBodyStmt(os);
		break;
	default:
		this->logger->error("Unknown statement type");
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
	case pesto::ParamTilingType::LAST_DIM_RECT:
		if (this->tilingDepth == this->group.tilingData.size() - 1) {
			this->genHeaderStmtRectangular(os);
		} else {
			this->genHeaderStmtAlgebraic(os);
		}
		break;
	default:
		this->logger->error("Unknown tiling type");
	}
}

void ParamTilingCodegen::genHeaderStmtRectangular(std::ostream &os) {
	os << this->nameHelper.outerForUBName() << " = ceild(" << this->data.innerForUBExpr;
	if (this->data.innerForLBExpr.length() > 0) {
		os << " - (" << this->data.innerForLBExpr << ")";
	}
	os << ", " << this->nameHelper.divName() << ")";
}

void ParamTilingCodegen::genHeaderStmtAlgebraic(std::ostream &os) {
	isl_size npar = isl_set_dim(this->innerDomain, isl_dim_param);
	os << this->nameHelper.outerForUBName() << " = " << this->nameHelper.divName() << " - 1;" << this->endline;
	os << this->nameHelper.pcMaxName() << " = " << this->trahrheNameHelper.ehrhartFnName() << "(";
	for (isl_size i = 0; i < npar; i += 1) {
		if (i != 0) {
			os << ", ";
		}
		os << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	for (std::size_t i = 0; i < this->tilingDepth; i += 1) {
		auto dataIt = std::find_if(this->group.tilingData.begin(), this->group.tilingData.end(),
								   [i](const std::unique_ptr<TilingData> &d) {
									   size_t depth = static_cast<size_t>(d->tiling_depth);
									   return depth == i;
								   });
		assert(dataIt != this->group.tilingData.end());
		auto &data = *dataIt;
		os << ", " << data->nameHelper->innerForLBName();
		os << ", " << data->nameHelper->innerForUBName();
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
			os << "\t" << this->nameHelper.outerForUBName() << " = 0;" << this->endline;
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
			os << "\t" << this->nameHelper.outerForUBName() << " = " << dynDividerName << " - 1;" << this->endline;
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
	case pesto::ParamTilingType::LAST_DIM_RECT:
		if (this->tilingDepth == this->group.tilingData.size() - 1) {
			this->genBodyStmtRectangular(os);
		} else {
			this->genBodyStmtAlgebraic(os);
		}
		break;
	default:
		this->logger->error("Unknown tiling type");
	}
}

void ParamTilingCodegen::genBodyStmtRectangular(std::ostream &os) {
	os << this->nameHelper.innerForLBName() << " = " << this->nameHelper.divName() << " * " << this->outerFor.iterator;
	os << "+" << this->data.innerForLBExpr << ";" << this->endline;

	os << this->nameHelper.innerForUBName() << " = " << this->nameHelper.divName() << " * " << this->outerFor.iterator
	   << " + " << this->nameHelper.divName() << " - 1";
	os << "+" << this->data.innerForLBExpr;
}

void ParamTilingCodegen::genBodyStmtAlgebraic(std::ostream &os) {
	isl_size npar = isl_set_dim(this->innerDomain, isl_dim_param);

	// inner for lower bound
	os << this->nameHelper.innerForLBName() << " = ";
	os << this->trahrheNameHelper.trahrheFnName() << "(max(1, (" << this->outerFor.iterator << ") * "
	   << this->nameHelper.targetVolName() << ")";
	for (isl_size i = 0; i < npar; i += 1) {
		os << ", " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}

	for (size_t i = 0; i < this->tilingDepth; i += 1) {
		auto dataIt = std::find_if(this->group.tilingData.begin(), this->group.tilingData.end(),
								   [i](const std::unique_ptr<TilingData> &d) {
									   size_t depth = static_cast<size_t>(d->tiling_depth);
									   return depth == i;
								   });
		assert(dataIt != this->group.tilingData.end());
		auto &data = *dataIt;
		os << ", " << data->nameHelper->innerForLBName();
		os << ", " << data->nameHelper->innerForUBName();
	}
	os << ")";
	if (this->boundComputeMethod == BoundComputeMethod::FILL_FIRST) {
		os << " + !!" << this->outerFor.iterator;
	}
	os << ";" << this->endline;
	// inner for upper bound
	os << this->nameHelper.innerForUBName() << " = ";
	os << this->trahrheNameHelper.trahrheFnName() << "(min(" << this->nameHelper.pcMaxName() << ", ("
	   << this->outerFor.iterator << " + 1) * " << this->nameHelper.targetVolName() << ")";
	for (isl_size i = 0; i < npar; i += 1) {
		os << ", " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	for (std::size_t i = 0; i < this->tilingDepth; i += 1) {
		auto dataIt = std::find_if(this->group.tilingData.begin(), this->group.tilingData.end(),
								   [i](const std::unique_ptr<TilingData> &d) {
									   size_t depth = static_cast<size_t>(d->tiling_depth);
									   return depth == i;
								   });
		assert(dataIt != this->group.tilingData.end());
		auto &data = *dataIt;
		os << ", " << data->nameHelper->innerForLBName();
		os << ", " << data->nameHelper->innerForUBName();
	}
	os << ")";
	if (this->boundComputeMethod == BoundComputeMethod::FILL_LAST) {
		os << " - 1";
	}
	// last bound adjustment
	if (this->boundComputeMethod == BoundComputeMethod::FILL_LAST) {
		os << ";" << this->endline;
		os << "if(" << this->outerFor.iterator << " == " << this->nameHelper.outerForUBName() << ") {" << this->endline;
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

void ParamTilingCodegen::setEndline(std::string endline) {
	this->endline = endline;
}

void ParamTilingCodegen::setVolumeCheckThreshold(std::string threshold) {
	if (threshold.empty()) {
		this->logger->warn("Volume check threshold is empty. Default to {}", this->volumeCheckThreshold.c_str());
		return;
	}
	this->volumeCheckThreshold = threshold;
}

void ParamTilingCodegen::genHeaderStmtScheduled(std::ostream &os) {
	isl_size npar = isl_set_dim(this->innerDomain, isl_dim_param);

	if (this->tilingDepth == 0) {
		std::string tileArrayName = this->trahrheNameHelper.tileArrayName();
		std::string scheduleName = this->trahrheNameHelper.scheduleName();
		os << "SCHEDULE_ALLOC(" << tileArrayName << ", " << scheduleName << ");" << this->endline;
		os << "SCHEDULE_COMPUTE(" << this->trahrheNameHelper.getPrefix() << ", " << tileArrayName << ", "
		   << scheduleName;
		for (isl_size i = 0; i < npar; i += 1) {
			os << ", " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
		}
		os << ");" << this->endline;

		os << this->nameHelper.outerForUBName() << " = " << scheduleName << ".len - 1;" << this->endline;
	}
	if (this->tilingDepth == 1) {
		auto dataIt = std::find_if(this->group.tilingData.begin(), this->group.tilingData.end(),
								   [](const std::unique_ptr<TilingData> &d) {
									   size_t depth = static_cast<size_t>(d->tiling_depth);
									   return depth == 0;
								   });
		assert(dataIt != this->group.tilingData.end());
		auto &data = *dataIt;
		std::string tileArrayName = data->trahrheNameHelper->tileArrayName();
		std::string scheduleName = data->trahrheNameHelper->scheduleName();
		os << this->nameHelper.outerForUBName() << " = " << scheduleName << ".fronts["
		   << this->group.tilingData[0]->outer_for->iterator << "].len - 1;" << this->endline;
	}
}

void ParamTilingCodegen::genBodyStmtScheduled(std::ostream &os) {
	if (this->tilingDepth == 0) {
		return;
	}
	if (this->tilingDepth > 1) {
		this->logger->warn("Scheduled tiling depth is > 1. This is really weird...");
		return;
	}

	auto dataIt = std::find_if(this->group.tilingData.begin(), this->group.tilingData.end(),
							   [](const std::unique_ptr<TilingData> &d) {
								   size_t depth = static_cast<size_t>(d->tiling_depth);
								   return depth == 0;
							   });
	assert(dataIt != this->group.tilingData.end());
	auto &outerData = *dataIt;
	std::string tileArrayName = outerData->trahrheNameHelper->tileArrayName();
	std::string scheduleName = outerData->trahrheNameHelper->scheduleName();

	std::string scheduleAccessName =
		scheduleName + ".fronts[" + outerData->outer_for->iterator + "].tiles[" + this->outerFor.iterator + "]";

	os << outerData->nameHelper->innerForLBName() << " = " << scheduleAccessName << "->lb0;" << this->endline;
	os << outerData->nameHelper->innerForUBName() << " = " << scheduleAccessName << "->ub0;" << this->endline;
	os << this->nameHelper.innerForLBName() << " = " << scheduleAccessName << "->lb1;" << this->endline;
	os << this->nameHelper.innerForUBName() << " = " << scheduleAccessName << "->ub1;" << this->endline;
}

void ParamTilingCodegen::genScheduleHeaderFile(std::ostream &os) {
	// sanity check
	if (this->tilingDepth != 0) {
		this->logger->error("Schedule header file can only be generated for tiling depth 0");
		return;
	}
	if (this->group.tilingData.size() < 2) {
		this->logger->error("Schedule header file can only be generated for tiling depth 0 and 1");
		return;
	}
	if (this->tilingType != ParamTilingType::SCHEDULED) {
		this->logger->error("Schedule header file can only be generated for scheduled tiling");
		return;
	}
	// done sanity check
	// generate the header file

	std::string headerMacro = "PESTO_RUNTIME_SCHEDULE_" + this->trahrheNameHelper.getPrefix() + "_H";
	std::string varType = "long int";
	isl_size npar = isl_set_dim(this->innerDomain, isl_dim_param);

	TilingData &t1Data = *this->group.tilingData[1];
	// multi include guard
	os << "#ifndef " << headerMacro << std::endl;
	os << "#define " << headerMacro << std::endl;

	// copy template schedule.h
	std::ifstream scheduleTemplate(_PESTO_TEMPLATE_SCHEDULE_PATH, std::ios::in);
	if (!scheduleTemplate.is_open()) {
		this->logger->error("Cannot open schedule template file");
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
	os << varType << " " << t1Data.nameHelper->pcMaxName() << ", " << t1Data.nameHelper->targetVolName() << ";"
	   << this->endline;

	INDENT_TABS(os, 1);

	INDENT_TABS(os, 1);
	os << varType << " " << this->nameHelper.innerForLBName() << ", " << this->nameHelper.innerForUBName() << ";"
	   << this->endline;

	os << "#pragma omp parallel for firstprivate(" << this->nameHelper.pcMaxName() << ", "
	   << this->nameHelper.targetVolName() << ") " //
	   << "private(" << t1Data.nameHelper->pcMaxName() << ", " << t1Data.nameHelper->targetVolName() << ", "
	   << this->nameHelper.innerForLBName() << ", " << this->nameHelper.innerForUBName() << ")" << this->endline;
	INDENT_TABS(os, 1);
	os << "for(" << varType << " __pesto_t0 = 0; __pesto_t0 < " << this->nameHelper.divName() << "; ++__pesto_t0) {"
	   << this->endline;
	INDENT_TABS(os, 2);
	os << varType << " " << this->nameHelper.innerForLBName() << " = " << this->trahrheNameHelper.trahrheFnName()
	   << "(max(1, __pesto_t0 * " << this->nameHelper.targetVolName() << ")";
	for (isl_size i = 0; i < npar; i += 1) {
		os << ", " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	os << ");" << this->endline;

	INDENT_TABS(os, 2);
	os << varType << " " << this->nameHelper.innerForUBName() << " = " << this->trahrheNameHelper.trahrheFnName()
	   << "(min(" << this->nameHelper.pcMaxName() << ", (__pesto_t0 + 1) * " << this->nameHelper.targetVolName() << ")";
	for (isl_size i = 0; i < npar; i += 1) {
		os << ", " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	os << ") - 1;" << this->endline;

	INDENT_TABS(os, 2);
	os << "if(__pesto_t0 == " << this->nameHelper.divName() << " - 1) {" << this->endline;
	INDENT_TABS(os, 3);
	os << this->nameHelper.innerForUBName() << " = " << this->data.innerForUBExpr << ";" << this->endline;
	INDENT_TABS(os, 2);
	os << "}" << this->endline;

	INDENT_TABS(os, 2);
	os << t1Data.nameHelper->pcMaxName() << " = " << t1Data.trahrheNameHelper->ehrhartFnName() << "(";
	for (isl_size i = 0; i < npar; i += 1) {
		if (i != 0) {
			os << ", ";
		}
		os << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	os << ", " << this->nameHelper.innerForLBName() << ", " << this->nameHelper.innerForUBName() << ");"
	   << this->endline;

	INDENT_TABS(os, 2);
	os << t1Data.nameHelper->targetVolName() << " = " << t1Data.nameHelper->pcMaxName() << " / "
	   << t1Data.nameHelper->divName() << ";" << this->endline;

	INDENT_TABS(os, 2);
	os << "for(" << varType << " __pesto_t1 = 0; __pesto_t1 < " << t1Data.nameHelper->divName() << "; ++__pesto_t1) {"
	   << this->endline;

	INDENT_TABS(os, 3);
	os << "tileArray[__pesto_t0][__pesto_t1].t0 = __pesto_t0;" << this->endline;
	INDENT_TABS(os, 3);
	os << "tileArray[__pesto_t0][__pesto_t1].t1 = __pesto_t1;" << this->endline;
	INDENT_TABS(os, 3);
	os << "tileArray[__pesto_t0][__pesto_t1].lb0 = " << this->nameHelper.innerForLBName() << ";" << this->endline;
	INDENT_TABS(os, 3);
	os << "tileArray[__pesto_t0][__pesto_t1].ub0 = " << this->nameHelper.innerForUBName() << ";" << this->endline;
	INDENT_TABS(os, 3);
	os << "tileArray[__pesto_t0][__pesto_t1].lb1 = " << t1Data.trahrheNameHelper->trahrheFnName()
	   << "(max(1, __pesto_t1 * " << t1Data.nameHelper->targetVolName() << ")";
	for (isl_size i = 0; i < npar; i += 1) {
		os << ", " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	os << ", " << this->nameHelper.innerForLBName() << ", " << this->nameHelper.innerForUBName() << ");"
	   << this->endline;
	INDENT_TABS(os, 3);
	os << "tileArray[__pesto_t0][__pesto_t1].ub1 = " << t1Data.trahrheNameHelper->trahrheFnName() << "(min("
	   << t1Data.nameHelper->pcMaxName() << ", (__pesto_t1 + 1) * " << t1Data.nameHelper->targetVolName() << ")";
	for (isl_size i = 0; i < npar; i += 1) {
		os << ", " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	os << ", " << this->nameHelper.innerForLBName() << ", " << this->nameHelper.innerForUBName() << ") - 1;"
	   << this->endline;
	INDENT_TABS(os, 3);
	os << "tileArray[__pesto_t0][__pesto_t1].marked = 0;" << this->endline;
	INDENT_TABS(os, 3);
	os << "if(__pesto_t1 == " << t1Data.nameHelper->divName() << " - 1) {" << this->endline;
	INDENT_TABS(os, 4);
	os << "tileArray[__pesto_t0][__pesto_t1].ub1 = " << (*this->group.tilingData[1]).innerForUBExpr << ";"
	   << this->endline;
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

NameHelper::NameHelper(int depth, std::string outerForIt, std::string innerForIt)
	: depth(depth)
	, sep_("_")
	, prefix_("")
	, outerForIt_(outerForIt)
	, innerForIt_(innerForIt) //
{}

NameHelper::~NameHelper() {}

std::string &NameHelper::getSeparator() {
	return this->sep_;
}
std::string &NameHelper::getPrefix() {
	return this->prefix_;
}
std::string NameHelper::getPrefixStr() {
	return (this->prefix_ != "" ? this->prefix_ + this->sep_ : "");
}

void NameHelper::setSeparator(std::string sep) {
	this->sep_ = sep;
}
void NameHelper::setPrefix(std::string prefix) {
	this->prefix_ = prefix;
}

std::string NameHelper::outerForLBName() {
	return this->getPrefixStr() + "lb" + this->outerForIt_;
}
std::string NameHelper::outerForUBName() {
	return this->getPrefixStr() + "ub" + this->outerForIt_;
}
std::string NameHelper::innerForLBName() {
	return this->getPrefixStr() + "lb" + this->innerForIt_;
}
std::string NameHelper::innerForUBName() {
	return this->getPrefixStr() + "ub" + this->innerForIt_;
}
std::string NameHelper::divName() {
	return "DIV" + std::to_string(this->depth);
}
std::string NameHelper::targetVolName() {
	return this->getPrefixStr() + "TARGET" + this->sep_ + "VOL" + this->sep_ + "L" + std::to_string(this->depth);
}
std::string NameHelper::pcMaxName() {
	return this->trahrheIteratorName() + this->sep_ + "pcmax";
}
std::string NameHelper::ehrhartFnName() {
	std::string itName = this->trahrheIteratorName();
	return itName + this->sep_ + "Ehrhart";
}
std::string NameHelper::trahrheFnName() {
	std::string itName = this->trahrheIteratorName();
	return itName + this->sep_ + "trahrhe" + this->sep_ + itName;
}
std::string NameHelper::trahrheIteratorName() {
	return this->getPrefixStr() + this->innerForIt_;
}

std::string NameHelper::scheduleName() {
	return this->getPrefixStr() + "schedule";
}
std::string NameHelper::tileArrayName() {
	return this->getPrefixStr() + "tiles";
}

std::string NameHelper::innerForIteratorName() {
	return this->innerForIt_;
}