#include <stdlib.h>
#include <string.h>

#include <pesto/trahrhe.h>

/* codegen functions */
void trahrhe_codegen_header_stmt_algebraic(FILE *fp,
										   struct trahrhe_codegen_data *data);
void trahrhe_codegen_body_stmt_algebraic(FILE *fp,
										 struct trahrhe_codegen_data *data);
void trahrhe_codegen_header_stmt_rectangular(FILE *fp,
											 struct trahrhe_codegen_data *data);
void trahrhe_codegen_body_stmt_rectangular(FILE *fp,
										   struct trahrhe_codegen_data *data);
void trahrhe_codegen_header_stmt_schedule(FILE *fp,
										  struct trahrhe_codegen_data *data);
void trahrhe_codegen_body_stmt_schedule(FILE *fp,
										struct trahrhe_codegen_data *data);

void trahrhe_codegen_header_stmt(FILE *fp, struct trahrhe_codegen_data *data) {
	switch (data->tiling_type) {
	case trahrhe_type_algebraic:
		trahrhe_codegen_header_stmt_algebraic(fp, data);
		break;
	case trahrhe_type_rectangular:
		trahrhe_codegen_header_stmt_rectangular(fp, data);
		break;
	case trahrhe_type_scheduled:
		trahrhe_codegen_header_stmt_schedule(fp, data);
		break;
	default:
		fprintf(stderr, "Not implemented yet\n");
		exit(1);
	}
}

void trahrhe_codegen_body_stmt(FILE *fp, struct trahrhe_codegen_data *data) {
	switch (data->tiling_type) {
	case trahrhe_type_algebraic:
		trahrhe_codegen_body_stmt_algebraic(fp, data);
		break;
	case trahrhe_type_rectangular:
		trahrhe_codegen_body_stmt_rectangular(fp, data);
		break;
	case trahrhe_type_scheduled:
		trahrhe_codegen_body_stmt_schedule(fp, data);
		break;
	default:
		fprintf(stderr, "Not implemented yet\n");
		exit(1);
	}
}

void trahrhe_codegen_stmt(FILE *fp, struct trahrhe_codegen_data *data) {
	switch (data->stmt_type) {
	case trahrhe_stmt_head:
		trahrhe_codegen_header_stmt(fp, data);
		break;
	case trahrhe_stmt_body:
		trahrhe_codegen_body_stmt(fp, data);
		break;
	}
}

void trahrhe_codegen_header_stmt_algebraic(FILE *fp,
										   struct trahrhe_codegen_data *data) {
	/*    Tiling upper bound computation     aub0t = DIV0 - 1;   */
	TRAHRHE_GEN_OUTTER_UB(fprintf, fp, data);
	fprintf(fp, " = ");
	TRAHRHE_GEN_DIV(fprintf, fp, data);
	fprintf(fp, " - 1;\\\n"); // new line

	TRAHRHE_GEN_PCMAX(fprintf, fp, data);
	fprintf(fp, " = ");
	TRAHRHE_GEN_ERHART_FN(fprintf, fp, data);
	fprintf(fp, "(");
	for (int i = 0; i < data->npar; i++) {
		fprintf(fp, "%s", data->params[i]);
		if (i != data->npar - 1) {
			fprintf(fp, ", ");
		}
	}
	// for (int i = 0; i < data->depth; i++) {
	// 	fprintf(fp, ", %slb%d, %sub%d", data->var_prefix, i, data->var_prefix,
	// 			i);
	// }
	fprintf(fp, ");");
	fprintf(fp, "\\\n"); // new line
	TRAHRHE_GEN_TARGET_VOL(fprintf, fp, data);
	fprintf(fp, " = ");
	TRAHRHE_GEN_PCMAX(fprintf, fp, data);
	fprintf(fp, " / ");
	TRAHRHE_GEN_DIV(fprintf, fp, data);
}

void trahrhe_codegen_body_stmt_algebraic(FILE *fp,
										 struct trahrhe_codegen_data *data) {
	// lower bound
	TRAHRHE_GEN_INNER_LB(fprintf, fp, data);
	fprintf(fp, " = ");
	TRAHRHE_GEN_TRAHRHE_FN(fprintf, fp, data);
	fprintf(fp, "(max(1, (%s) * ", data->iterator_name);
	TRAHRHE_GEN_TARGET_VOL(fprintf, fp, data);
	fprintf(fp, ")");

	// minimum args are prog parameters
	for (int i = 0; i < data->npar; i++) {
		fprintf(fp, ", %s", data->params[i]);
	}
	// append bounds of previous loops
	// for (int i = 0; i < data->depth; i++) {
	// 	fprintf(fp, ", %slb%u, %sub%u", data->var_prefix, i, data->var_prefix,
	// 			i);
	// }

	fprintf(fp, ");");
	fprintf(fp, "\\\n"); // new line

	// upper bound
	TRAHRHE_GEN_INNER_UB(fprintf, fp, data);
	fprintf(fp, " = ");
	TRAHRHE_GEN_TRAHRHE_FN(fprintf, fp, data);
	fprintf(fp, "(min(");
	TRAHRHE_GEN_PCMAX(fprintf, fp, data);
	fprintf(fp, ", (%s + 1) * ", data->iterator_name);
	TRAHRHE_GEN_TARGET_VOL(fprintf, fp, data);
	fprintf(fp, ")");

	// minimum args are prog parameters
	for (int i = 0; i < data->npar; i++) {
		fprintf(fp, ", %s", data->params[i]);
	}
	// append bounds of previous loops
	// for (int i = 0; i < data->depth; i++) {
	// 	fprintf(fp, ", %slb%d, %sub%d", data->var_prefix, i, data->var_prefix,
	// 			i);
	// }
	fprintf(fp, ") - 1;");
	fprintf(fp, "\\\n"); // new line

	// last bound adjustment
	fprintf(fp, "if(%s ==", data->iterator_name);
	TRAHRHE_GEN_DIV(fprintf, fp, data);
	fprintf(fp, " - 1) {");

	TRAHRHE_GEN_INNER_UB(fprintf, fp, data);
	fprintf(fp, " = %s", data->ub_expr);
	fprintf(fp, ";\\\n"); // new line

	fprintf(fp, "}");
}

void trahrhe_codegen_header_stmt_rectangular(
	FILE *fp, struct trahrhe_codegen_data *data) {
	fprintf(fp, "tub%s = ceild(", data->iterator_name);
	fprintf(fp, "%s", data->ub_expr);
	fprintf(fp, "- (%s - DIV%d - 1)", data->lb_expr, data->depth);
	fprintf(fp, ", DIV%u);", data->depth);
	fprintf(fp, "\\\n"); // new line
}
void trahrhe_codegen_body_stmt_rectangular(FILE *fp,
										   struct trahrhe_codegen_data *data) {
	fprintf(fp, "lb%s = ", data->inner_it_name);
	fprintf(fp, "DIV%u * %s", data->depth, data->iterator_name);
	fprintf(fp, " + %s - DIV%u - 1;", data->lb_expr, data->depth);
	fprintf(fp, "\\\n"); // new line

	fprintf(fp, "ub%s = ", data->inner_it_name);
	fprintf(fp, "DIV%u * %s", data->depth, data->iterator_name);
	fprintf(fp, " + DIV%u - 1", data->depth);
	fprintf(fp, " + %s - DIV%u - 1;", data->lb_expr, data->depth);
	fprintf(fp, "\\\n"); // new line
}

void trahrhe_codegen_free(struct trahrhe_codegen_data *data) {
	if (data->lb_expr) {
		free(data->lb_expr);
	}
	if (data->ub_expr) {
		free(data->ub_expr);
	}
	if (data->lb_name) {
		free(data->lb_name);
	}
	if (data->ub_name) {
		free(data->ub_name);
	}
	if (data->endline) {
		free(data->endline);
	}
	if (data->iterator_name) {
		free(data->iterator_name);
	}
	if (data->inner_it_name) {
		free(data->inner_it_name);
	}
	if (data->params) {
		for (int i = 0; i < data->npar; i++) {
			free(data->params[i]);
		}
		free(data->params);
	}
}

void trahrhe_codegen_header_stmt_schedule(FILE *fp,
										  struct trahrhe_codegen_data *data) {
	if (data->depth > 1) {
		// we have a problem here
		return;
	}
	if (data->depth == 0) {
		TRAHRHE_GEN_OUTTER_UB(fprintf, fp, data);
		fprintf(fp, " = schedule.len - 1;\\\n");
		return;
	}
	if (data->depth == 1) {

		return;
	}
}
void trahrhe_codegen_body_stmt_schedule(FILE *fp,
										struct trahrhe_codegen_data *data) {
	if (data->depth > 1) {
		// we have a problem here
		return;
	}
	if (data->depth == 0) {
		TRAHRHE_GEN_OUTTER_UB(fprintf, fp, data);
		fprintf(fp, " = schedule.fronts[%s].len - 1;\\\n", data->iterator_name);
	}
}
