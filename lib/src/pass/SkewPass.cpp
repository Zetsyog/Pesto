#include <pesto/pass/SkewPass.hpp>
#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/ir/Scop2Isl.hpp>

#include <isl/aff.h>
#include <isl/constraint.h>
#include <isl/flow.h>
#include <isl/local_space.h>
#include <isl/map.h>
#include <isl/printer.h>
#include <isl/set.h>
#include <isl/space.h>
#include <isl/union_map.h>
#include <isl/union_set.h>

#include <osl/osl.h>

#include <stdexcept>

using namespace pesto::pass;

SkewPass::SkewPass() : IPass("skew") {}
SkewPass::~SkewPass() {}

std::set<std::string> SkewPass::inputIR() const { return {"OpenScopIR"}; }
std::string SkewPass::outputIR() const { return "OpenScopIR"; }

void SkewPass::setTransform(const std::vector<std::string> &lines) {
	transform_ = SkewTransform::parse(lines);
	haveTransform_ = true;
}

osl_relation_p SkewPass::skewStatementScattering(isl_ctx *ctx, osl_statement_p stmt,
												 const std::vector<std::string> &paramNames) {
	(void)ctx;
	(void)paramNames;

	// original loop iterators, in domain order
	std::vector<std::string> domainIters;
	osl_body_p stmtBody = (osl_body_p)osl_generic_lookup(stmt->extension, OSL_URI_BODY);
	int nbNames = stmtBody ? osl_strings_size(stmtBody->iterators) : 0;
	for (int i = 0; i < nbNames; i++)
		domainIters.push_back(stmtBody->iterators->string[i]);
	if (domainIters.empty()) {
		logger->error("skew: statement has no named iterators");
		return nullptr;
	}

	// Work on a copy of the existing (identity) scattering, keeping its exact
	// osl layout. We only rewrite the rows of the loop dimensions.
	osl_relation_p scat = osl_relation_clone(stmt->scattering);
	int prec = scat->precision;

	int nOut = scat->nb_output_dims;   // 7 for a 3-loop nest
	int nIn = scat->nb_input_dims;     // = #iterators
	int nParam = scat->nb_parameters;

	// Column layout of an osl relation row:
	//   [0]           : equality flag (0 = equality)
	//   [1 .. nOut]   : output dims  o0..o_{nOut-1}
	//   [nOut+1 .. nOut+nIn] : input dims (iterators), in domain order
	//   [.. +nParam]  : parameters
	//   [last]        : constant
	int outBase = 1;
	int inBase = 1 + nOut;

	// Map iterator name -> its input-dim index (0-based)
	auto inputIndexOf = [&](const std::string &name) -> int {
		for (int d = 0; d < (int)domainIters.size(); d++)
			if (domainIters[d] == name)
				return d;
		return -1;
	};

	// The loop output dims are the odd positions (1,3,5,...). The k-th loop
	// dim corresponds to the k-th target of the transform.
	int loopIdx = 0;
	for (int o = 0; o < nOut; o++) {
		if (o % 2 == 0)
			continue; // scalar dim, leave as-is (o = 0)

		if (loopIdx >= (int)transform_.targets().size()) {
			logger->error("skew: more loop dims than transform targets");
			osl_relation_free(scat);
			return nullptr;
		}
		const SkewTarget &t = transform_.targets().at(loopIdx);
		loopIdx++;

		// Find the row that defines this output dim o: it has a nonzero entry
		// in column (outBase + o). In the identity it is -1.
		int row = -1;
		for (int r = 0; r < scat->nb_rows; r++) {
			int v = osl_int_get_si(prec, scat->m[r][outBase + o]);
			if (v != 0) { row = r; break; }
		}
		if (row < 0) {
			logger->error("skew: could not find the row defining output dim {}", o);
			osl_relation_free(scat);
			return nullptr;
		}

		// Convention (from the identity): output coeff is -1, so the constraint
		// is  -o + <expr> = 0, i.e. o = <expr>. We therefore place the target
		// coefficients directly into the input columns, and clear params/const.
		// 1) force output coeff to -1
		osl_int_set_si(prec, &scat->m[row][outBase + o], -1);
		// 2) clear all input coefficients on this row
		for (int d = 0; d < nIn; d++)
			osl_int_set_si(prec, &scat->m[row][inBase + d], 0);
		// 3) clear parameter and constant columns on this row
		for (int p = 0; p < nParam; p++)
			osl_int_set_si(prec, &scat->m[row][inBase + nIn + p], 0);
		osl_int_set_si(prec, &scat->m[row][scat->nb_columns - 1], 0);
		// 4) set the target coefficients
		for (const auto &kv : t.coeffs) {
			int idx = inputIndexOf(kv.first);
			if (idx < 0) {
				logger->error("skew: iterator '{}' is not a loop iterator of this statement",
							  kv.first);
				osl_relation_free(scat);
				return nullptr;
			}
			osl_int_set_si(prec, &scat->m[row][inBase + idx], kv.second);
		}
	}

	return scat;
}

void SkewPass::checkCone(isl_ctx *ctx, osl_scop_p scop) {
	using pesto::ir::converter::Scop2Isl;
	Scop2Isl irHelper(ctx);
	irHelper.convert(scop);
	
	isl_union_map *schedules = irHelper.getSchedules();
	
	isl_union_map *reads = irHelper.getReads();
	isl_union_map *writes = irHelper.getWrites();

	isl_union_access_info *access = isl_union_access_info_from_sink(isl_union_map_copy(reads));
	access = isl_union_access_info_set_must_source(access, isl_union_map_copy(writes));
	access = isl_union_access_info_set_schedule_map(access, isl_union_map_copy(schedules));
	isl_union_flow *flow = isl_union_access_info_compute_flow(access);
	isl_union_map *dep = isl_union_flow_get_must_dependence(flow);
	isl_union_flow_free(flow);
	
	// Map dependences through the schedule so deltas are computed in the SKEWED
	// scattering space, not the original (i,j,k) domain. Without this, the
	// deltas reflect the un-skewed geometry.
	dep = isl_union_map_apply_domain(dep, isl_union_map_copy(schedules));
	dep = isl_union_map_apply_range(dep, isl_union_map_copy(schedules));

	isl_union_set *deltas = isl_union_map_deltas(dep);

	struct Acc { isl_set *set; } acc{nullptr};
	// After mapping through the schedule, deltas live in the 7-dim scattering
	// space [0, x, 0, y, 0, z, 0]. The loop axes we want are dims 1 (x) and 3 (y).
	isl_union_set_foreach_set(
		deltas,
		+[](__isl_take isl_set *d, void *user) -> isl_stat {
			auto *a = static_cast<Acc *>(user);
			int nd = isl_set_dim(d, isl_dim_set);
			if (nd < 4) { isl_set_free(d); return isl_stat_ok; }
			// keep dims 1 and 3 (x and y), drop the rest
			// drop dim 3's successors first, then dim 2, then dim 0
			d = isl_set_project_out(d, isl_dim_set, 4, nd - 4); // drop >=4
			d = isl_set_project_out(d, isl_dim_set, 2, 1);       // drop dim 2 (scalar)
			d = isl_set_project_out(d, isl_dim_set, 0, 1);       // drop dim 0 (scalar)
			// now dims are (x, y)
			d = isl_set_set_tuple_name(d, nullptr);
			a->set = a->set ? isl_set_union(a->set, d) : d;
			return isl_stat_ok;
		},
		&acc);

	isl_union_set_free(deltas);
	isl_union_map_free(schedules);
	isl_union_map_free(reads);
	isl_union_map_free(writes);

	if (!acc.set) {
		logger->warn("skew: no dependence vectors found; cannot verify the cone");
		return;
	}
	acc.set = isl_set_coalesce(acc.set);

	isl_space *sp = isl_set_get_space(acc.set);
	isl_set *cone = isl_set_universe(isl_space_copy(sp));
	isl_local_space *lls = isl_local_space_from_space(sp);
	isl_constraint *c1 = isl_constraint_alloc_inequality(isl_local_space_copy(lls));
	c1 = isl_constraint_set_coefficient_si(c1, isl_dim_set, 1, 1);      // dy >= 0
	cone = isl_set_add_constraint(cone, c1);
	isl_constraint *c2 = isl_constraint_alloc_inequality(isl_local_space_copy(lls));
	c2 = isl_constraint_set_coefficient_si(c2, isl_dim_set, 0, 1);      // dx - dy >= 0
	c2 = isl_constraint_set_coefficient_si(c2, isl_dim_set, 1, -1);
	cone = isl_set_add_constraint(cone, c2);
	isl_local_space_free(lls);

	isl_set *outside = isl_set_subtract(isl_set_copy(acc.set), cone);
	int empty = isl_set_is_empty(outside);
	if (!empty) {
		char *s = isl_set_to_str(outside);
		std::string msg = std::string("skew: transform does not qualify for trapezoidal tiling; "
									   "distance vectors outside the (1,0)-(1,1) cone: ") + (s ? s : "?");
		free(s);
		isl_set_free(outside);
		isl_set_free(acc.set);
		logger->error("{}", msg);
		throw std::runtime_error(msg);
	}
	isl_set_free(outside);
	isl_set_free(acc.set);
	logger->info("skew: all distance vectors lie in the (1,0)-(1,1) cone");
}

pesto::Bundle SkewPass::apply(pesto::Bundle &&bundle) {
	if (!haveTransform_) {
		logger->error("skew: no transform specified");
		throw std::runtime_error("skew: no transform specified");
	}

	ir::OpenScopIR *scopIR = dynamic_cast<ir::OpenScopIR *>(bundle.get_ir());
	osl_scop_p scop = scopIR->c_scop();

	// parameter names from the scop context
	std::vector<std::string> paramNames;
	if (scop->parameters && scop->parameters->data) {
		osl_strings_p params = (osl_strings_p)scop->parameters->data;
		int np = osl_strings_size(params);
		for (int p = 0; p < np; p++)
			paramNames.push_back(params->string[p]);
	}

	isl_ctx *ctx = isl_ctx_alloc();
	
	for (osl_statement_p stmt = scop->statement; stmt; stmt = stmt->next) {
		osl_relation_p newScat = skewStatementScattering(ctx, stmt, paramNames);
		if (!newScat) {
			isl_ctx_free(ctx);
			throw std::runtime_error("skew: failed to skew a statement scattering");
		}
		osl_relation_free(stmt->scattering);
		stmt->scattering = newScat;
	}
	
	checkCone(ctx, scop);

	isl_ctx_free(ctx);
	return std::move(bundle);
}

