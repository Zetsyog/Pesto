#ifndef PESTO_TRAHRHE_H
#define PESTO_TRAHRHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

/* codegen macros */
// outter loop upper bound
#define TRAHRHE_GEN_OUTTER_UB(fn, target, data)                                \
	fn(target, "g%d_ub%s", (data)->group_id, (data)->iterator_name)

// DIVi
#define TRAHRHE_GEN_DIV(fn, target, data) fn(target, "DIV%u", (data)->depth)

// TARGET_VOL_Li
#define TRAHRHE_GEN_TARGET_VOL(fn, target, data)                               \
	fn(target, "g%d_TARGET_VOL_L%d", (data)->group_id, (data)->depth)

#define TRAHRHE_GEN_PCMAX(fn, target, data)                                    \
	fn(target, "g%d_%s_pcmax", (data)->group_id, (data)->inner_it_name)

#define TRAHRHE_GEN_ERHART_FN(fn, target, data)                                \
	fn(target, "n%d_g%d_%s_Ehrhart", (data)->nest_id, (data)->group_id,        \
	   (data)->inner_it_name)

#define TRAHRHE_GEN_TRAHRHE_FN(fn, target, data)                               \
	fn(target, "n%d_g%d_%s_trahrhe_n%d_g%d_%s", (data)->nest_id,               \
	   (data)->group_id, (data)->inner_it_name, (data)->nest_id,               \
	   (data)->group_id, (data)->inner_it_name)

#define TRAHRHE_GEN_INNER_LB(fn, target, data)                                 \
	fn(target, "g%d_lb%s", (data)->group_id, (data)->inner_it_name)
#define TRAHRHE_GEN_INNER_UB(fn, target, data)                                 \
	fn(target, "g%d_ub%s", (data)->group_id, (data)->inner_it_name)

#define TRAHRHE_GEN_TRAHRHE_ITERATOR(fn, target, data)                         \
	fn(target, "n%d_g%d_%s", (data)->nest_id, (data)->group_id,                \
	   (data)->inner_it_name)

enum trahrhe_stmt_type { trahrhe_stmt_head, trahrhe_stmt_body };

enum ptile_type {
	trahrhe_type_rectangular,
	trahrhe_type_algebraic,
};

struct trahrhe_codegen_data {
	enum trahrhe_stmt_type stmt_type;
	enum ptile_type tiling_type;
	int nest_id;
	int stmt_id;
	int group_id;
	char *lb_name;
	char *ub_name;
	char *inner_it_name;
	unsigned int depth; /* current tiling depth */
	char *iterator_name;
	char *lb_expr;
	char *ub_expr;
	int npar;
	char **params;
	char *endline;
};

void trahrhe_codegen_stmt(FILE *fp, struct trahrhe_codegen_data *data);

void trahrhe_codegen_header_stmt_schedule(FILE *fp,
										  struct trahrhe_codegen_data *data,
										  struct trahrhe_codegen_data *pair);
void trahrhe_codegen_body_stmt_schedule(FILE *fp,
										struct trahrhe_codegen_data *data,
										struct trahrhe_codegen_data *pair);

void trahrhe_codegen_free(struct trahrhe_codegen_data *data);

#ifdef __cplusplus
}
#endif

#endif // PESTO_TRAHRHE_H