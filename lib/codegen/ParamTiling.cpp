#include <stdlib.h>
#include <string.h>

#include <isl/aff.h>
#include <isl/set.h>

#include <pesto/Pesto.hpp>
#include <pesto/clast/clast_support.h>

using namespace pesto::ptile::codegen;

ParamTilingCodegen::ParamTilingCodegen(StmtGroup &group, struct clast_for &outterFor, struct clast_for &innerFor,
									   int tilingDepth, int nestId, __isl_keep isl_set *innerDomain)
	: group(group)
	, tilingType(ParamTilingType::ALGEBRAIC)
	, outterFor(outterFor)
	, innerFor(innerFor)
	, tilingDepth(tilingDepth)
	, innerDomain(nullptr)
	, boundComputeMethod(BoundComputeMethod::FILL_LAST)
	, endline(std::string("\\\n"))
	, boundCorrectionStrategy(BoundCorrectionStrategy::NO_CORRECTION)
	, volumeCheckStrategy(VolumeCheckStrategy::NO_CHECK)
	, innerForLBExpr("")
	, innerForUBExpr("")
	, nameHelper(tilingDepth, outterFor.iterator, innerFor.iterator)
	, trahrheNameHelper(tilingDepth, outterFor.iterator, innerFor.iterator) {
	this->innerDomain = isl_set_copy(innerDomain);
	isl_set_free(innerDomain);
	this->nameHelper.setPrefix("g" + std::to_string(group.id));
	this->trahrheNameHelper.setPrefix("n" + std::to_string(nestId) + this->nameHelper.getSeparator() + "g" +
									  std::to_string(group.id));
	this->computeBoundExpr();
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

void ParamTilingCodegen::computeBoundExpr() {
	std::string innerForIt = std::string(this->innerFor.iterator);
	std::string outterForIt = std::string(this->outterFor.iterator);

	// [0] is for lower bound, [1] is for upper bound
	CloogOptions *cloogOptions = cloog_options_malloc(nullptr);
	CloogNames *namesList[2] = {nullptr, nullptr};
	isl_size npar = isl_set_dim(this->innerDomain, isl_dim_param);
	for (std::size_t i = 0; i < 2; ++i) {
		namesList[i] = cloog_names_malloc();
		if (namesList[i] == NULL) {
			std::cerr << "[param-tiling] Error: cannot allocate memory for cloog names" << std::endl;
			return;
		}
		CloogNames *names = namesList[i];
		names->nb_parameters = npar;
		names->parameters = (char **)calloc(npar, sizeof(char *));
		for (isl_size j = 0; j < npar; j += 1) {
			names->parameters[j] = strdup(isl_set_get_dim_name(this->innerDomain, isl_dim_param, j));
		}
		names->nb_scattering = this->group.tilingData.size();
		names->scattering = (char **)calloc(names->nb_scattering, sizeof(char *));
		for (isl_size j = 0; j < names->nb_scattering; j += 1) {
			auto &data = this->group.tilingData[j];
			NameHelper helper = NameHelper(data.tiling_depth, data.outter_for->iterator, data.inner_for->iterator);
			helper.setPrefix(this->nameHelper.getPrefix());
			helper.setSeparator(this->nameHelper.getSeparator());
			std::string bound = i == 0 ? helper.innerForLBName() : helper.innerForUBName();
			names->scattering[j] = strdup(bound.c_str());
		}
	}

	char *buffer = (char *)calloc(512, sizeof(char));
	FILE *fd = fmemopen(buffer, 512, "w");
	for (std::size_t i = 0; i < 2; ++i) {
		struct clast_expr *expr = clast_bound_from_isl_set(namesList[i], this->innerDomain, i, this->tilingDepth);
		if (expr != NULL) {
			memset(buffer, 0, 512);
			rewind(fd);
			clast_pprint_expr(cloogOptions, fd, expr);
			fflush(fd);
			if (i == 0) {
				this->innerForLBExpr = std::string(buffer);
			} else {
				this->innerForUBExpr = std::string(buffer);
			}
		}
		free_clast_expr(expr);
	}
	fclose(fd);
	free(buffer);

	for (std::size_t i = 0; i < 2; ++i) {
		cloog_names_free(namesList[i]);
	}
	cloog_options_free(cloogOptions);
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
	default:
		std::cerr << "[param-tiling] Unknown tiling type" << std::endl;
	}
}

void ParamTilingCodegen::genHeaderStmtRectangular(std::ostream &os) {
	os << this->nameHelper.outterForUBName() << " = ceild(" << this->innerForUBExpr;
	if (this->innerForLBExpr.length() > 0) {
		os << " - (" << this->innerForLBExpr << ")";
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
		std::string volumeThreshold = this->nameHelper.divName() + " * 5";
		os << "if(" << this->nameHelper.targetVolName() << " <= " << volumeThreshold << ") {" << this->endline;

		if (this->volumeCheckStrategy == VolumeCheckStrategy::CHECK_AND_WARN ||
			this->volumeCheckStrategy == VolumeCheckStrategy::CHECK_AND_DIE) {
			os << "\tfprintf(stderr, \"Warning: computed volume is too small (" << this->nameHelper.targetVolName()
			   << " <= " << volumeThreshold
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
	default:
		std::cerr << "[param-tiling] Unknown tiling type" << std::endl;
	}
}

void ParamTilingCodegen::genBodyStmtRectangular(std::ostream &os) {
	os << this->nameHelper.innerForLBName() << " = " << this->nameHelper.divName() << " * " << this->outterFor.iterator;
	os << "+" << this->innerForLBExpr << ";" << this->endline;

	os << this->nameHelper.innerForUBName() << " = " << this->nameHelper.divName() << " * " << this->outterFor.iterator
	   << " + " << this->nameHelper.divName() << " - 1";
	os << "+" << this->innerForLBExpr;
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
		os << "if(" << this->outterFor.iterator << " == " << this->nameHelper.divName() << " - 1) {" << this->endline;
		os << "\t" << this->nameHelper.innerForUBName() << " = " << this->innerForUBExpr << ";" << this->endline;
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
