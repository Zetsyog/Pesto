#ifndef PESTO_TRAHRHE_H
#define PESTO_TRAHRHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

enum trahrhe_stmt_type { head, body };

enum ptile_type {
	rectangular,
	algebraic,
	algebraic_standalone,
	rectangular_standalone
};

struct trahrhe_codegen_data {
	enum trahrhe_stmt_type stmt_type;
	enum ptile_type tiling_type;
	int stmt_id;
	char *var_prefix;
	char *fun_prefix;
	unsigned int depth; /* current tiling depth */
	char *iterator_name;
	char *lb_expr;
	char *ub_expr;
	int npar;
	char **params;
	char *endline;
};

void trahrhe_codegen_stmt(FILE *fp, struct trahrhe_codegen_data *data);

#ifdef __cplusplus
}
#endif

#endif // PESTO_TRAHRHE_H