/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/Logging.hpp>
#include <pesto/ParamConstraint.hpp>

#include <sstream>
#include <stdexcept>
#include <vector>

#include <isl/constraint.h>
#include <isl/ctx.h>
#include <isl/options.h>
#include <isl/set.h>
#include <isl/val.h>

#include <osl/generic.h>
#include <osl/macros.h>
#include <osl/relation.h>
#include <osl/strings.h>
#include <osl/util.h>

namespace pesto {

namespace {

/** One affine row over the parameters: sum(coeffs[i] * P_i) + cst (== | >=) 0. */
struct ParamRow {
	bool isEq = false;
	std::vector<long> coeffs;
	long cst = 0;
};

std::vector<std::string> scopParamNames(osl_scop_p scop) {
	std::vector<std::string> names;
	int nparam = osl_scop_get_nb_parameters(scop);
	if (nparam <= 0) {
		return names;
	}
	auto *strings = (osl_strings_p)osl_generic_lookup(scop->parameters, OSL_URI_STRINGS);
	if (strings == nullptr || (int)osl_strings_size(strings) < nparam) {
		throw std::invalid_argument("param constraint: the scop has no parameter names");
	}
	for (int i = 0; i < nparam; i += 1) {
		names.emplace_back(strings->string[i]);
	}
	return names;
}

std::string paramTuple(const std::vector<std::string> &names) {
	std::string s = "[";
	for (size_t i = 0; i < names.size(); i += 1) {
		s += (i ? ", " : "") + names[i];
	}
	return s + "]";
}

std::vector<ParamRow> lowerBoundRows(const std::vector<int> &bounds) {
	std::vector<ParamRow> rows;
	for (size_t i = 0; i < bounds.size(); i += 1) {
		ParamRow row;
		row.coeffs.assign(bounds.size(), 0);
		row.coeffs[i] = 1;
		row.cst = -bounds[i];
		rows.push_back(std::move(row));
	}
	return rows;
}

long valToLong(isl_val *v, const std::string &expr) {
	if (v == nullptr || !isl_val_is_int(v)) {
		isl_val_free(v);
		throw std::invalid_argument("param constraint \"" + expr + "\": non-integer coefficient");
	}
	long r = isl_val_get_num_si(v);
	isl_val_free(v);
	return r;
}

std::vector<ParamRow> exprRows(const std::vector<std::string> &names, const std::string &expr) {
	isl_ctx *ctx = isl_ctx_alloc();
	isl_options_set_on_error(ctx, ISL_ON_ERROR_CONTINUE);

	std::string str = paramTuple(names) + " -> { : " + expr + " }";
	isl_set *set = isl_set_read_from_str(ctx, str.c_str());

	auto fail = [&](const std::string &why) {
		isl_set_free(set);
		isl_ctx_free(ctx);
		throw std::invalid_argument("param constraint \"" + expr + "\": " + why);
	};

	if (set == nullptr) {
		fail("cannot parse it (unknown parameter or invalid isl syntax); scop parameters are " +
			 paramTuple(names));
	}
	if ((int)isl_set_dim(set, isl_dim_param) != (int)names.size()) {
		fail("it names parameters that are not in the scop " + paramTuple(names));
	}
	set = isl_set_coalesce(set);
	isl_size nbset = isl_set_n_basic_set(set);
	if (nbset == 0) {
		fail("it is unsatisfiable");
	}
	if (nbset != 1) {
		fail("disjunctive constraints ('or') are not supported");
	}

	isl_basic_set_list *list = isl_set_get_basic_set_list(set);
	isl_basic_set *bset = isl_basic_set_list_get_at(list, 0);
	isl_basic_set_list_free(list);
	if (isl_basic_set_dim(bset, isl_dim_div) != 0) {
		isl_basic_set_free(bset);
		fail("constraints needing existential variables (e.g. modulo) are not supported");
	}

	struct Data {
		std::vector<ParamRow> rows;
		const std::vector<std::string> *names;
		const std::string *expr;
		std::string error;
	} data{{}, &names, &expr, {}};

	isl_basic_set_foreach_constraint(
		bset,
		[](isl_constraint *c, void *user) -> isl_stat {
			auto *d = static_cast<Data *>(user);
			try {
				ParamRow row;
				row.isEq = isl_constraint_is_equality(c);
				for (size_t i = 0; i < d->names->size(); i += 1) {
					row.coeffs.push_back(
						valToLong(isl_constraint_get_coefficient_val(c, isl_dim_param, (int)i), *d->expr));
				}
				row.cst = valToLong(isl_constraint_get_constant_val(c), *d->expr);
				d->rows.push_back(std::move(row));
			} catch (const std::exception &e) {
				d->error = e.what();
				isl_constraint_free(c);
				return isl_stat_error;
			}
			isl_constraint_free(c);
			return isl_stat_ok;
		},
		&data);

	isl_basic_set_free(bset);
	isl_set_free(set);
	isl_ctx_free(ctx);

	if (!data.error.empty()) {
		throw std::invalid_argument(data.error);
	}
	return data.rows;
}

void appendRows(osl_relation_p rel, const std::vector<ParamRow> &rows) {
	int nparam = rel->nb_parameters;
	int base = 1 + rel->nb_output_dims + rel->nb_input_dims + rel->nb_local_dims;
	for (const ParamRow &row : rows) {
		int r = rel->nb_rows;
		osl_relation_insert_blank_row(rel, r);
		osl_int_set_si(rel->precision, &rel->m[r][0], row.isEq ? 0 : 1);
		for (int i = 0; i < nparam; i += 1) {
			osl_int_set_si(rel->precision, &rel->m[r][base + i], row.coeffs[i]);
		}
		osl_int_set_si(rel->precision, &rel->m[r][rel->nb_columns - 1], row.cst);
	}
}

/** Returns true if the context is known to be empty. */
bool contextIsEmpty(osl_relation_p context, const std::vector<std::string> &names) {
	std::ostringstream os;
	os << paramTuple(names) << " -> { ";
	bool firstPart = true;
	for (osl_relation_p rel = context; rel != nullptr; rel = rel->next) {
		if (rel->nb_output_dims || rel->nb_input_dims || rel->nb_local_dims) {
			return false; // not worth handling, just skip the check
		}
		os << (firstPart ? "" : "; ") << ": ";
		firstPart = false;
		bool firstRow = true;
		for (int r = 0; r < rel->nb_rows; r += 1) {
			os << (firstRow ? "" : " and ") << "0";
			firstRow = false;
			for (int i = 0; i < rel->nb_parameters; i += 1) {
				os << " + (" << osl_int_get_si(rel->precision, rel->m[r][1 + i]) << ")*" << names[i];
			}
			os << " + (" << osl_int_get_si(rel->precision, rel->m[r][rel->nb_columns - 1]) << ")";
			os << (osl_int_zero(rel->precision, rel->m[r][0]) ? " = 0" : " >= 0");
		}
		if (firstRow) {
			os << "true";
		}
	}
	os << " }";

	isl_ctx *ctx = isl_ctx_alloc();
	isl_options_set_on_error(ctx, ISL_ON_ERROR_CONTINUE);
	isl_set *set = isl_set_read_from_str(ctx, os.str().c_str());
	bool empty = set != nullptr && isl_set_is_empty(set) == isl_bool_true;
	isl_set_free(set);
	isl_ctx_free(ctx);
	return empty;
}

} // namespace

std::string ParamConstraint::str() const {
	if (std::holds_alternative<std::monostate>(spec)) {
		return "none";
	}
	if (auto *v = std::get_if<int>(&spec)) {
		return "every parameter >= " + std::to_string(*v);
	}
	if (auto *m = std::get_if<PerParam>(&spec)) {
		std::string s;
		for (auto &[name, v] : *m) {
			s += (s.empty() ? "" : ", ") + name + " >= " + std::to_string(v);
		}
		return s + (s.empty() ? "" : ", ") + "others >= 0";
	}
	return std::get<std::string>(spec);
}

void applyParamConstraint(osl_scop_p scop, const ParamConstraint &constraint) {
	if (scop == nullptr || constraint.isNone()) {
		return;
	}
	auto logger = log::get_logger("param-constraint");

	int nparam = osl_scop_get_nb_parameters(scop);
	if (nparam == 0 && !std::holds_alternative<std::string>(constraint.spec)) {
		return;
	}

	std::vector<std::string> names;
	std::vector<ParamRow> rows;

	if (auto *v = std::get_if<int>(&constraint.spec)) {
		rows = lowerBoundRows(std::vector<int>(nparam, *v));
	} else if (auto *m = std::get_if<ParamConstraint::PerParam>(&constraint.spec)) {
		names = scopParamNames(scop);
		std::vector<int> bounds(nparam, 0);
		for (auto &[name, v] : *m) {
			bool found = false;
			for (int i = 0; i < nparam; i += 1) {
				if (names[i] == name) {
					bounds[i] = v;
					found = true;
				}
			}
			if (!found) {
				throw std::invalid_argument("param constraint: unknown parameter '" + name +
											"'; scop parameters are " + paramTuple(names));
			}
		}
		rows = lowerBoundRows(bounds);
	} else {
		names = scopParamNames(scop);
		rows = exprRows(names, std::get<std::string>(constraint.spec));
	}

	if (scop->context == nullptr) {
		scop->context = osl_relation_pmalloc(osl_util_get_precision(), 0, nparam + 2);
		osl_relation_set_attributes(scop->context, 0, 0, 0, nparam);
		osl_relation_set_type(scop->context, OSL_TYPE_CONTEXT);
	}
	for (osl_relation_p rel = scop->context; rel != nullptr; rel = rel->next) {
		if (rel->nb_parameters != nparam) {
			throw std::runtime_error("param constraint: scop context has an unexpected number of parameters");
		}
		appendRows(rel, rows);
	}

	logger->debug("Added parameter constraint to the scop context: {}", constraint.str());

	if (nparam > 0) {
		if (names.empty()) {
			try {
				names = scopParamNames(scop);
			} catch (const std::exception &) {
				return;
			}
		}
		if (contextIsEmpty(scop->context, names)) {
			logger->warn("Parameter constraint '{}' makes the scop context empty: no code will be generated.",
						 constraint.str());
		}
	}
}

} // namespace pesto
