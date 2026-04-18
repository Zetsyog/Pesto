/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <isl/aff.h>
#include <isl/ast_build.h>
#include <isl/flow.h>
#include <isl/maybe_pw_aff.h>
#include <isl/multi.h>
#include <isl/printer.h>
#include <isl/schedule.h>
#include <isl/schedule_node.h>
#include <isl/union_set.h>
#include <isl/val.h>

#include <pesto/ir/Isl2Scop.hpp>
#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/ir/Scop2Isl.hpp>
#include <pesto/macros.h>
#include <pesto/pass/IslPass.hpp>

using pesto::pass::IslPass;
using namespace pesto;

IslPass::IslPass() : IPass("isl-test") {
	this->ctx = isl_ctx_alloc();
}

IslPass::~IslPass() {
	isl_ctx_free(this->ctx);
}

/* Construct an isl_multi_val living in "space" with all values equal to "val".
 */
static __isl_give isl_multi_val *ppcg_multi_val_from_int(__isl_take isl_space *space, int val) {
	int i, n;
	isl_ctx *ctx;
	isl_val *v;
	isl_multi_val *mv;

	if (!space)
		return NULL;

	ctx = isl_space_get_ctx(space);
	n = isl_space_dim(space, isl_dim_set);
	mv = isl_multi_val_zero(space);
	v = isl_val_int_from_si(ctx, val);
	for (i = 0; i < n; ++i) {
		if (i == 0) {
			isl_val *tmp = isl_val_int_from_si(ctx, 11);
			mv = isl_multi_val_set_val(mv, i, tmp);
		} else if (i == 1) {
			isl_val *tmp = isl_val_int_from_si(ctx, 13);
			mv = isl_multi_val_set_val(mv, i, tmp);
		} else if (i == 2) {
			isl_val *tmp = isl_val_int_from_si(ctx, 17);
			mv = isl_multi_val_set_val(mv, i, tmp);
		} else {
			mv = isl_multi_val_set_val(mv, i, isl_val_copy(v));
		}
	}
	isl_val_free(v);

	return mv;
}

/* Does "tile" represent a trivial lattice tile, i.e.,
 * one with size 1 in all directions?
 */
static isl_bool lattice_is_trivial(__isl_keep isl_fixed_box *tile) {
	isl_bool trivial;
	isl_val *val_one;
	isl_multi_val *size;
	isl_multi_val *zero, *one;

	size = isl_fixed_box_get_size(tile);
	zero = isl_multi_val_sub(isl_multi_val_copy(size), isl_multi_val_copy(size));
	val_one = isl_val_one(isl_fixed_box_get_ctx(tile));
	one = isl_multi_val_add_val(zero, val_one);
	trivial = isl_multi_val_plain_is_equal(size, one);
	isl_multi_val_free(size);
	isl_multi_val_free(one);

	return trivial;
}

/* Given that the elements of "set" lie on a (rectangular) lattice
 * with tile "tile", scale it down such that the origin of the result
 * corresponds to "tile".
 *
 * The lattice is of the form
 *
 *	offset + size j
 *
 * Plug this into "set" to obtain a set in terms of "j"
 * with j = 0 corresponding to offset in "set".
 */
static __isl_give isl_set *scale_down_set(__isl_keep isl_fixed_box *tile, __isl_take isl_set *set) {
	isl_space *space;
	isl_multi_val *size;
	isl_multi_aff *offset;
	isl_multi_aff *id;
	isl_multi_aff *to_lattice;

	size = isl_fixed_box_get_size(tile);
	offset = isl_fixed_box_get_offset(tile);
	space = isl_multi_aff_get_space(offset);
	id = isl_space_identity_multi_aff_on_domain(isl_space_copy(space));
	offset = isl_multi_aff_insert_domain(offset, space);
	to_lattice = isl_multi_aff_scale_multi_val(id, size);
	to_lattice = isl_multi_aff_add(to_lattice, offset);
	set = isl_set_preimage_multi_aff(set, to_lattice);

	return set;
}

/* Given that the schedule values of the band node "node"
 * lie on a (rectangular) lattice with tile "tile",
 * scale it down such that the values corresponding to "tile"
 * are mapped to the origin.
 * "domain" is the (universe) domain reaching "node".
 *
 * The lattice is of the form
 *
 *	offset + size j
 *
 * Subtract offset and scale down by size.
 */
static __isl_give isl_schedule_node *scale_down_band(__isl_keep isl_fixed_box *tile, __isl_take isl_schedule_node *node,
													 __isl_keep isl_union_set *domain) {
	isl_multi_val *size;
	isl_multi_aff *offset;
	isl_multi_union_pw_aff *mupa;

	size = isl_fixed_box_get_size(tile);
	offset = isl_multi_aff_neg(isl_fixed_box_get_offset(tile));
	domain = isl_union_set_copy(domain);
	mupa = isl_multi_union_pw_aff_multi_aff_on_domain(domain, offset);
	node = isl_schedule_node_band_shift(node, mupa);
	node = isl_schedule_node_band_scale_down(node, size);

	return node;
}

/* Try and shift the given band node to the origin after
 * potentially scaling it down first.
 *
 * In particular, obtain the set of schedule values and
 * first check if they lie on a non-trivial (rectangular) lattice.
 * If so, scale down both the band node and the set.  Then
 * compute the element-wise minimal value, which may
 * depend on the parameters.
 * If this results in any piecewise expressions,
 * then do not perform any shifting as that may
 * very well make the resulting code more complicated.
 * Otherwise shift the band by the opposite of this minimal value.
 */
static __isl_give isl_schedule_node *scale_down_and_shift_to_origin(__isl_take isl_schedule_node *node) {
	isl_bool is_multi_aff, trivial;
	isl_space *space;
	isl_union_set *domain, *range;
	isl_union_map *partial;
	isl_set *min_domain;
	isl_set *set;
	isl_multi_pw_aff *min;
	isl_multi_aff *min_ma, *shift;
	isl_multi_union_pw_aff *mupa;
	isl_fixed_box *tile;

	partial = isl_schedule_node_band_get_partial_schedule_union_map(node);
	domain = isl_schedule_node_get_domain(node);
	range = isl_union_set_apply(isl_union_set_copy(domain), partial);
	space = isl_schedule_node_band_get_space(node);
	set = isl_union_set_extract_set(range, space);
	isl_union_set_free(range);
	domain = isl_union_set_universe(domain);

	tile = isl_set_get_lattice_tile(set);
	trivial = lattice_is_trivial(tile);
	if (trivial < 0) {
		node = isl_schedule_node_free(node);
	} else if (!trivial) {
		set = scale_down_set(tile, set);
		node = scale_down_band(tile, node, domain);
	}
	isl_fixed_box_free(tile);

	min = isl_set_min_multi_pw_aff(set);
	min_domain = isl_multi_pw_aff_domain(isl_multi_pw_aff_copy(min));
	min = isl_multi_pw_aff_gist(min, min_domain);
	is_multi_aff = isl_multi_pw_aff_isa_multi_aff(min);
	if (is_multi_aff < 0 || !is_multi_aff) {
		isl_union_set_free(domain);
		isl_multi_pw_aff_free(min);
		if (is_multi_aff < 0)
			return isl_schedule_node_free(node);
		return node;
	}

	min_ma = isl_multi_pw_aff_as_multi_aff(min);
	shift = isl_multi_aff_neg(min_ma);
	mupa = isl_multi_union_pw_aff_multi_aff_on_domain(domain, shift);
	node = isl_schedule_node_band_shift(node, mupa);

	return node;
}

Bundle IslPass::apply(Bundle &&bundle) {
	using pesto::ir::converter::Isl2Scop;
	using pesto::ir::converter::Scop2Isl;

	auto *scopIR = dynamic_cast<pesto::ir::OpenScopIR *>(bundle.get_ir());

	isl_printer *p = isl_printer_to_file(ctx, stderr);

	osl_scop_p c_scop = scopIR->c_scop();

	Scop2Isl irHelper(this->ctx);
	irHelper.convert(c_scop);

	isl_set *context = irHelper.getContext();
	isl_union_set *domains = irHelper.getDomains();
	isl_union_map *schedules = irHelper.getSchedules();
	isl_union_map *writes = irHelper.getWrites();
	isl_union_map *mayWrites = irHelper.getMayWrites();
	isl_union_map *reads = irHelper.getReads();
	isl_union_map *empty = isl_union_map_empty(irHelper.getParamSpace());

	isl_union_access_info *access;
	isl_union_flow *flow;

	isl_union_map *dep_flow;  // potential flow dependences
	isl_union_map *dep_false; // potential false (anti and output) dependences

	// compute flow dependences
	access = isl_union_access_info_from_sink(isl_union_map_copy(reads));
	access = isl_union_access_info_set_kill(access, isl_union_map_copy(writes));
	access = isl_union_access_info_set_may_source(access, isl_union_map_copy(mayWrites));
	access = isl_union_access_info_set_schedule_map(access, isl_union_map_copy(schedules));
	flow = isl_union_access_info_compute_flow(access);
	dep_flow = isl_union_flow_get_may_dependence(flow);

	isl_union_flow_free(flow);

	// compute false dependences
	isl_union_map *may_source = isl_union_map_union(isl_union_map_copy(mayWrites), isl_union_map_copy(reads));
	access = isl_union_access_info_from_sink(isl_union_map_copy(mayWrites));
	access = isl_union_access_info_set_kill(access, isl_union_map_copy(writes));
	access = isl_union_access_info_set_may_source(access, may_source);
	access = isl_union_access_info_set_schedule_map(access, isl_union_map_copy(schedules));
	flow = isl_union_access_info_compute_flow(access);
	dep_false = isl_union_flow_get_may_dependence(flow);
	dep_false = isl_union_map_coalesce(dep_false);

	isl_union_flow_free(flow);

	isl_options_set_ast_always_print_block(ctx, isl_bool_true);
	isl_options_set_ast_print_outermost_block(ctx, isl_bool_false);

	isl_options_set_tile_scale_tile_loops(ctx, isl_bool_false);
	isl_options_set_tile_shift_point_loops(ctx, isl_bool_false); // whether point loops start at 0
	// isl_options_set_schedule_carry_self_first(ctx, isl_bool_true);
	isl_options_set_schedule_whole_component(ctx, isl_bool_true);
	isl_options_set_schedule_maximize_coincidence(ctx, isl_bool_true);
	isl_options_set_schedule_maximize_band_depth(ctx, isl_bool_true);
	// try to construct schedules where the outermost scheduling dimension in each band
	// satisfies the coincidence constraints
	isl_options_set_schedule_outer_coincidence(ctx, isl_bool_true);

	// raw = isl_union_access_info_set_schedule_map(isl_union_access_info *access, isl_union_map *schedule_map)

	isl_schedule_constraints *sc = isl_schedule_constraints_on_domain(isl_union_set_copy(domains));
	sc = isl_schedule_constraints_set_context(sc, isl_set_copy(context));

	isl_union_map *validity = isl_union_map_union(isl_union_map_copy(dep_flow), isl_union_map_copy(dep_false));
	isl_union_map *coincidence = isl_union_map_copy(validity);
	isl_union_map *proximity = isl_union_map_copy(dep_flow);

	sc = isl_schedule_constraints_set_validity(sc, validity);
	sc = isl_schedule_constraints_set_coincidence(sc, coincidence);
	sc = isl_schedule_constraints_set_proximity(sc, proximity);

	isl_schedule *schedule = isl_schedule_constraints_compute_schedule(sc);

	schedule = isl_schedule_map_schedule_node_bottom_up(
		schedule,
		[](__isl_take isl_schedule_node *node, void *user) -> __isl_give isl_schedule_node * {
			(void)user;
			if (isl_schedule_node_get_type(node) != isl_schedule_node_band) {
				return node;
			}
			int n = isl_schedule_node_band_n_member(node);
			if (n <= 1) {
				return node;
			}
			isl_space *space = isl_schedule_node_band_get_space(node);
			isl_multi_val *sizes = ppcg_multi_val_from_int(space, 32);

			isl_schedule_node_dump(node);
			node = scale_down_and_shift_to_origin(node);
			node = isl_schedule_node_band_tile(node, sizes);
			n = isl_schedule_node_band_n_member(node);
			for (int i = 0; i < n; ++i)
				node = isl_schedule_node_band_member_set_ast_loop_type(node, i, isl_ast_loop_atomic);

			return node;
		},
		nullptr);

	p = isl_printer_print_schedule(p, schedule);

	isl_ast_build *build = isl_ast_build_alloc(this->ctx);

	isl_ast_node *tree = isl_ast_build_node_from_schedule(build, isl_schedule_copy(schedule));
	isl_ast_build_free(build);

	isl_ast_print_options *options = isl_ast_print_options_alloc(this->ctx);

	p = isl_printer_flush(p);
	p = isl_printer_end_line(p);
	p = isl_printer_set_output_format(p, ISL_FORMAT_C);
	p = isl_ast_node_print(tree, p, options);

	isl_ast_node_free(tree);

	Isl2Scop isl2scop;

	fprintf(stderr, "ISL2SCOP\n");
	isl2scop.applySchedule(c_scop, schedule);

	isl_union_set_free(domains);
	isl_union_map_free(schedules);
	isl_union_map_free(writes);
	isl_union_map_free(mayWrites);
	isl_union_map_free(reads);
	isl_union_map_free(empty);

	isl_union_map_free(dep_flow);
	isl_union_map_free(dep_false);

	isl_printer_free(p);
	isl_schedule_free(schedule);
	isl_set_free(context);

	return std::move(bundle);
}