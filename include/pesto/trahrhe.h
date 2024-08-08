#ifndef PESTO_TRAHRHE_H
#define PESTO_TRAHRHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

enum trahrhe_stmt_type { trahrhe_stmt_head, trahrhe_stmt_body };

enum ptile_type {
	trahrhe_type_rectangular,
	trahrhe_type_algebraic,
	trahrhe_type_algebraic_standalone,
	trahrhe_type_rectangular_standalone
};

struct trahrhe_codegen_data {
	enum trahrhe_stmt_type stmt_type;
	enum ptile_type tiling_type;
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

void trahrhe_codegen_free(struct trahrhe_codegen_data *data);

#ifdef __cplusplus
}
#endif

#endif // PESTO_TRAHRHE_H