#include <stdlib.h>
#include <string.h>

#include <pesto/ParamTiling.hpp>
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
	, nestId(nestId)
	, innerDomain(nullptr)
	, boundComputeMethod(BoundComputeMethod::FILL_LAST)
	, endline(std::string("\\\n")) {
	this->innerDomain = isl_set_copy(innerDomain);
	this->computeNames();
}

ParamTilingCodegen::~ParamTilingCodegen() {
	isl_set_free(this->innerDomain);
	this->innerDomain = nullptr;
}

void ParamTilingCodegen::setTilingType(pesto::ParamTilingType type) { this->tilingType = type; }

void ParamTilingCodegen::setBoundComputeMethod(BoundComputeMethod method) { this->boundComputeMethod = method; }

std::string &ParamTilingCodegen::getGroupPrefixName() { return this->groupPrefixName; }

std::string &ParamTilingCodegen::getNestPrefixName() { return this->nestPrefixName; }

std::string &ParamTilingCodegen::getOutterForUBName() { return this->outterForUBName; }
std::string &ParamTilingCodegen::getInnerForLBName() { return this->innerForLBName; }
std::string &ParamTilingCodegen::getInnerForUBName() { return this->innerForUBName; }
std::string &ParamTilingCodegen::getDivName() { return this->divName; }
std::string &ParamTilingCodegen::getTargetVolName() { return this->targetVolName; }
std::string &ParamTilingCodegen::getPCMaxName() { return this->pcMaxName; }
std::string &ParamTilingCodegen::getEhrhartFnName() { return this->ehrhartFnName; }
std::string &ParamTilingCodegen::getTrahrheFnName() { return this->trahrheFnName; }
std::string &ParamTilingCodegen::getTrahrheIteratorName() { return this->trahrheIteratorName; }

std::string genInnerForLBName(std::string &sep, int groupId, const std::string &innerForIt) {
	return "g" + std::to_string(groupId) + sep + "lb" + innerForIt;
}

std::string genInnerForUBName(std::string &sep, int groupId, const std::string &innerForIt) {
	return "g" + std::to_string(groupId) + sep + "ub" + innerForIt;
}

void ParamTilingCodegen::computeNames() {
	std::string innerForIt = std::string(this->innerFor.iterator);
	std::string outterForIt = std::string(this->outterFor.iterator);

	std::string sep = "_";

	this->groupPrefixName = "g" + std::to_string(this->group.id);
	this->nestPrefixName = "n" + std::to_string(this->nestId);
	this->outterForUBName = this->groupPrefixName + sep + "ub" + outterForIt;
	this->innerForLBName = genInnerForLBName(sep, group.id, innerForIt);
	this->innerForUBName = genInnerForUBName(sep, group.id, innerForIt);
	this->divName = "DIV" + std::to_string(this->tilingDepth);
	this->targetVolName =
		this->groupPrefixName + sep + "TARGET" + sep + "VOL" + sep + "L" + std::to_string(this->tilingDepth);
	this->pcMaxName = this->groupPrefixName + sep + innerForIt + sep + "pcmax";
	this->ehrhartFnName = this->nestPrefixName + sep + this->groupPrefixName + sep + innerForIt + sep + "Ehrhart";
	this->trahrheFnName = this->nestPrefixName + sep + this->groupPrefixName + sep + innerForIt + sep + "trahrhe" +
						  sep + this->nestPrefixName + sep + this->groupPrefixName + sep + innerForIt;
	this->trahrheIteratorName = this->nestPrefixName + sep + this->groupPrefixName + sep + innerForIt;

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
			std::string bound = i == 0 ? genInnerForLBName(sep, group.id, std::string(data->inner_for->iterator))
									   : genInnerForUBName(sep, group.id, std::string(data->inner_for->iterator));
			names->scattering[j] = strdup(bound.c_str());
		}
	}

	char *buffer = (char *)calloc(512, sizeof(char));
	FILE *fd = fmemopen(buffer, 512, "w");
	for (std::size_t i = 0; i < 2; ++i) {
		struct clast_expr *expr = clast_bound_from_isl_set(namesList[i], this->innerDomain, i, this->tilingDepth);
		if (expr != NULL) {
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
	os << this->getOutterForUBName() << " = ceild(" << this->innerForUBExpr;
	if (this->innerForLBExpr.length() > 0) {
		os << " - (" << this->innerForLBExpr << ")";
	}
	os << ", " << this->getDivName() << ")";
}

void ParamTilingCodegen::genHeaderStmtAlgebraic(std::ostream &os) {
	isl_size npar = isl_set_dim(this->innerDomain, isl_dim_param);
	os << this->getOutterForUBName() << " = " << this->getDivName() << " - 1;" << this->endline;
	os << this->getPCMaxName() << " = " << this->getEhrhartFnName() << "(";
	for (isl_size i = 0; i < npar; i += 1) {
		if (i != 0) {
			os << ", ";
		}
		os << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	for (std::size_t i = 0; i < this->tilingDepth; i += 1) {
		auto &data = this->group.tilingData[i];
		os << ", " << data->codegen->getInnerForLBName();
		os << ", " << data->codegen->getInnerForUBName();
	}
	os << ");" << this->endline;
	os << this->getTargetVolName() << " = ";
	if (this->boundComputeMethod == BoundComputeMethod::FILL_FIRST) {
		os << "ceild(" << this->getPCMaxName() << ", " << this->getDivName() << ")";
	}
	if (this->boundComputeMethod == BoundComputeMethod::FILL_LAST) {
		os << this->getPCMaxName() << " / " << this->getDivName();
	}
	os << this->endline;
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
	os << this->getInnerForLBName() << " = " << this->getDivName() << " * " << this->outterFor.iterator;
	os << "+" << this->innerForLBExpr << ";" << this->endline;

	os << this->getInnerForUBName() << " = " << this->getDivName() << " * " << this->outterFor.iterator << " + "
	   << this->getDivName() << " - 1";
	os << "+" << this->innerForLBExpr;
}

void ParamTilingCodegen::genBodyStmtAlgebraic(std::ostream &os) {
	isl_size npar = isl_set_dim(this->innerDomain, isl_dim_param);

	// inner for lower bound
	os << this->getInnerForLBName() << " = ";
	os << this->getTrahrheFnName() << "(max(1, (" << this->outterFor.iterator << ") * " << this->getTargetVolName()
	   << ")";
	for (isl_size i = 0; i < npar; i += 1) {
		os << ", " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}

	for (std::size_t i = 0; i < this->tilingDepth; i += 1) {
		auto &data = this->group.tilingData[i];
		os << ", " << data->codegen->getInnerForLBName();
		os << ", " << data->codegen->getInnerForUBName();
	}
	os << ")";
	if (this->boundComputeMethod == BoundComputeMethod::FILL_FIRST) {
		os << " + !!" << this->outterFor.iterator;
	}
	os << ";" << this->endline;
	// inner for upper bound
	os << this->getInnerForUBName() << " = ";
	os << this->getTrahrheFnName() << "(min(" << this->getPCMaxName() << ", (" << this->outterFor.iterator << " + 1) * "
	   << this->getTargetVolName() << ")";
	for (isl_size i = 0; i < npar; i += 1) {
		os << ", " << isl_set_get_dim_name(this->innerDomain, isl_dim_param, i);
	}
	for (std::size_t i = 0; i < this->tilingDepth; i += 1) {
		auto &data = this->group.tilingData[i];
		os << ", " << data->codegen->getInnerForLBName();
		os << ", " << data->codegen->getInnerForUBName();
	}
	os << ")";
	if (this->boundComputeMethod == BoundComputeMethod::FILL_LAST) {
		os << " - 1";
	}
	// last bound adjustment
	if (this->boundComputeMethod == BoundComputeMethod::FILL_LAST) {
		os << ";" << this->endline;
		os << "if(" << this->outterFor.iterator << " == " << this->getDivName() << " - 1) {" << this->endline;
		os << "\t" << this->getInnerForUBName() << " = " << this->innerForUBExpr << ";" << this->endline;
		os << "}";
	}
}