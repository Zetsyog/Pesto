#ifndef PESTO_IR_ISL2SCOP_H
#define PESTO_IR_ISL2SCOP_H

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

#endif // PESTO_IR_ISL2SCOP_H
