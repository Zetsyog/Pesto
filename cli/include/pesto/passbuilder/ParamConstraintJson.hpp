/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <pesto/ParamConstraint.hpp>
#include <pesto/PassBuilder.hpp>

#include <limits>
#include <stdexcept>
#include <string>

#include <spdlog/logger.h>

namespace pesto::builder {

/**
 * Parses a parameter constraint option:
 *   null               -> no constraint
 *   integer v          -> every parameter >= v (-1 is the deprecated spelling of null)
 *   {"N": 11, ...}     -> per-parameter lower bounds, others >= 0
 *   "N > 10 and M > N" -> affine constraint in isl syntax
 */
inline ParamConstraint parseParamConstraint(const json &val, const std::string &key, spdlog::logger &logger) {
	auto toInt = [&](const json &v, const std::string &what) {
		if (!v.is_number_integer()) {
			logger.error("{}: {} must be an integer.", key, what);
			throw std::invalid_argument(key + ": " + what + " must be an integer");
		}
		auto i = v.get<long long>();
		if (i < std::numeric_limits<int>::min() || i > std::numeric_limits<int>::max()) {
			logger.error("{}: {} is out of range.", key, what);
			throw std::invalid_argument(key + ": " + what + " is out of range");
		}
		return (int)i;
	};

	if (val.is_null()) {
		return ParamConstraint::none();
	}
	if (val.is_number_integer()) {
		int v = toInt(val, "value");
		if (v == -1) {
			logger.warn("{}: -1 meaning 'no constraint' is deprecated, use null instead.", key);
			return ParamConstraint::none();
		}
		return ParamConstraint(v);
	}
	// Build the alternatives in place: moving a temporary Spec trips a GCC
	// -Wmaybe-uninitialized false positive on the variant storage.
	if (val.is_object()) {
		ParamConstraint result;
		auto &bounds = result.spec.emplace<ParamConstraint::PerParam>();
		for (auto &[name, v] : val.items()) {
			bounds[name] = toInt(v, "bound of '" + name + "'");
		}
		return result;
	}
	if (val.is_string()) {
		ParamConstraint result;
		result.spec.emplace<std::string>(val.get<std::string>());
		return result;
	}
	logger.error("{} must be null, an integer, an object of integers or a string.", key);
	throw std::invalid_argument(key + " must be null, an integer, an object of integers or a string");
}

} // namespace pesto::builder
