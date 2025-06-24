#ifndef PESTO_IR_SCOP2ISL_H
#define PESTO_IR_SCOP2ISL_H

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
	void convert(osl_scop_t &scop);

	__isl_give isl_space *getParamSpace();
	__isl_give isl_set *getContext();
	__isl_give isl_union_set *getDomains();
	__isl_give isl_union_map *getSchedules();
	__isl_give isl_union_map *getReads();
	__isl_give isl_union_map *getWrites();
	__isl_give isl_union_map *getMayWrites();

  private:
	isl_ctx *ctx;
	osl_scop_t *scop;

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

#endif