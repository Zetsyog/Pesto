/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <map>
#include <string>
#include <variant>

#include <osl/scop.h>

namespace pesto {

/**
 * Constraint on the global parameters of a scop, added to its context.
 *
 * - std::monostate: no constraint;
 * - int v: every parameter P satisfies P >= v;
 * - map {name: v}: P >= v for each listed parameter, P >= 0 for the others;
 * - string: an affine constraint over the parameters in isl syntax,
 *   e.g. "N > 10 and M > 25 and M >= N". It must be convex (no "or") and
 *   must not need existential variables.
 */
struct ParamConstraint {
	using PerParam = std::map<std::string, int>;
	using Spec = std::variant<std::monostate, int, PerParam, std::string>;

	Spec spec = 0;

	ParamConstraint() = default;
	ParamConstraint(Spec s) : spec(std::move(s)) {}

	static ParamConstraint none() {
		return ParamConstraint(std::monostate{});
	}

	bool isNone() const {
		return std::holds_alternative<std::monostate>(spec);
	}

	/** Human readable form, for logs. */
	std::string str() const;
};

/**
 * Adds the constraint to every union part of the scop context.
 * Throws std::invalid_argument on an unknown parameter name, an unparsable
 * expression, or a constraint that cannot be represented in an osl context.
 * Only the first scop of the list is updated.
 */
void applyParamConstraint(osl_scop_p scop, const ParamConstraint &constraint);

} // namespace pesto
