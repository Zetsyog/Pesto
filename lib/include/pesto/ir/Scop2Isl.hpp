/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <isl/ctx.h>
#include <isl/map.h>
#include <isl/space.h>
#include <isl/union_map.h>
#include <isl/union_set.h>

#include <osl/osl.h>

namespace pesto {

namespace ir::converter {

class Scop2Isl {

  public:
	Scop2Isl(isl_ctx *ctx);
	virtual ~Scop2Isl();

	/**
	 * Convert osl_scop_p to isl_union_set
	 * @param scop
	 */
	void convert(osl_scop_p scop);

	__isl_give isl_space *getParamSpace() const;
	__isl_give isl_set *getContext() const;
	__isl_give isl_union_set *getDomains() const;
	__isl_give isl_union_map *getSchedules() const;
	__isl_give isl_union_map *getReads() const;
	__isl_give isl_union_map *getWrites() const;
	__isl_give isl_union_map *getMayWrites() const;

  private:
	isl_ctx *ctx;
	osl_scop_p scop;

	isl_space *param_space;
	isl_set *context;
	isl_union_set *domain;
	isl_union_map *schedule;
	isl_union_map *reads;
	isl_union_map *writes;
	isl_union_map *may_writes;

	__isl_give isl_space *islDomainSpaceFromStatement(osl_statement_p stmt);
	__isl_give isl_set *islSetFromStatement(osl_statement_p stmt);
	__isl_give isl_map *islScheduleFromStatement(osl_statement_p stmt);
};

} // namespace ir::converter

} // namespace pesto
