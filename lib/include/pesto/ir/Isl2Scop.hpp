/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <isl/ctx.h>
#include <isl/schedule.h>

#include <osl/osl.h>

namespace pesto {

namespace ir::converter {

class Isl2Scop {
  public:
	Isl2Scop();
	virtual ~Isl2Scop();

	void applySchedule(osl_scop_p scop, isl_schedule *schedule);
};

} // namespace ir::converter
} // namespace pesto
