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

void trahrhe_codegen_header_stmt(FILE *fp, struct trahrhe_codegen_data *data) {
	switch (data->tiling_type) {
	case algebraic:
		trahrhe_codegen_header_stmt_algebraic(fp, data);
		break;
	case rectangular:
		trahrhe_codegen_header_stmt_rectangular(fp, data);
		break;
	default:
		fprintf(stderr, "Not implemented yet\n");
		exit(1);
	}
}

void trahrhe_codegen_body_stmt(FILE *fp, struct trahrhe_codegen_data *data) {
	switch (data->tiling_type) {
	case algebraic:
		trahrhe_codegen_body_stmt_algebraic(fp, data);
		break;
	case rectangular:
		trahrhe_codegen_body_stmt_rectangular(fp, data);
		break;
	default:
		fprintf(stderr, "Not implemented yet\n");
		exit(1);
	}
}

void trahrhe_codegen_stmt(FILE *fp, struct trahrhe_codegen_data *data) {
	switch (data->stmt_type) {
	case head:
		trahrhe_codegen_header_stmt(fp, data);
		break;
	case body:
		trahrhe_codegen_body_stmt(fp, data);
		break;
	}
}

void trahrhe_codegen_header_stmt_algebraic(FILE *fp,
										   struct trahrhe_codegen_data *data) {
	/*    Tiling upper bound computation     aub0t = DIV0 - 1;   */
	fprintf(fp, "%subt%u = DIV%u - 1; ", data->var_prefix, data->depth,
			data->depth);
	fprintf(fp, "\\\n"); // new line

	fprintf(fp, "%st%d_pcmax = ", data->var_prefix, data->depth);
	fprintf(fp, "%s_t%d_Ehrhart(", data->fun_prefix, data->depth);
	for (int i = 0; i < data->npar; i++) {
		fprintf(fp, "%s", data->params[i]);
		if (i != data->npar - 1) {
			fprintf(fp, ", ");
		}
	}
	for (int i = 0; i < data->depth; i++) {
		fprintf(fp, ", %slb%d, %sub%d", data->var_prefix, i, data->var_prefix,
				i);
	}
	fprintf(fp, ");");
	fprintf(fp, "\\\n"); // new line
	fprintf(fp, "%sTILE_VOL_L%d = ", data->var_prefix, data->depth);
	fprintf(fp, "%st%d_pcmax / DIV%u ", data->var_prefix, data->depth,
			data->depth);
}

void trahrhe_codegen_body_stmt_algebraic(FILE *fp,
										 struct trahrhe_codegen_data *data) {
	/*    Tiling upper bound computation     aub0t = DIV0 - 1;   */
	fprintf(fp, "%slb%u = ", data->var_prefix, data->depth);
	fprintf(fp, "%s_t%d_trahrhe_%s_t%d(", data->fun_prefix, data->depth,
			data->fun_prefix, data->depth);
	fprintf(fp, "max(1, (%s) * %sTILE_VOL_L%d)", data->iterator_name,
			data->var_prefix, data->depth);

	// minimum args are prog parameters
	for (int i = 0; i < data->npar; i++) {
		fprintf(fp, ", %s", data->params[i]);
	}
	// append bounds of previous loops
	for (int i = 0; i < data->depth; i++) {
		fprintf(fp, ", %slb%u, %sub%u", data->var_prefix, i, data->var_prefix,
				i);
	}

	fprintf(fp, ");");
	fprintf(fp, "\\\n"); // new line

	// upper bound
	fprintf(fp, "%sub%u = ", data->var_prefix, data->depth);
	fprintf(fp, "%s_t%d_trahrhe_%s_t%d(", data->fun_prefix, data->depth,
			data->fun_prefix, data->depth);
	fprintf(fp, "min(%st%d_pcmax, (%s + 1) * %sTILE_VOL_L%d)", data->var_prefix,
			data->depth, data->iterator_name, data->var_prefix, data->depth);
	// minimum args are prog parameters
	for (int i = 0; i < data->npar; i++) {
		fprintf(fp, ", %s", data->params[i]);
	}
	// append bounds of previous loops
	for (int i = 0; i < data->depth; i++) {
		fprintf(fp, ", %slb%d, %sub%d", data->var_prefix, i, data->var_prefix,
				i);
	}
	fprintf(fp, ") - 1;");
	fprintf(fp, "\\\n"); // new line

	// last bound adjustment
	fprintf(fp, "if(%s == DIV%u - 1) {\\\n", data->iterator_name, data->depth);

	fprintf(fp, "%sub%u = %s", data->var_prefix, data->depth, data->ub_expr);
	fprintf(fp, ";\\\n"); // new line

	fprintf(fp, "}");
}

void trahrhe_codegen_header_stmt_rectangular(
	FILE *fp, struct trahrhe_codegen_data *data) {
	fprintf(fp, "%subt%u = ceild(", data->var_prefix, data->depth);
	fprintf(fp, "%s", data->ub_expr);
	fprintf(fp, "- (%s - DIV%d - 1)", data->lb_expr, data->depth);
	fprintf(fp, ", DIV%u);", data->depth);
	fprintf(fp, "\\\n"); // new line
}
void trahrhe_codegen_body_stmt_rectangular(FILE *fp,
										   struct trahrhe_codegen_data *data) {
	fprintf(fp, "%slb%u = ", data->var_prefix, data->depth);
	fprintf(fp, "DIV%u * %s", data->depth, data->iterator_name);
	fprintf(fp, " + %s - DIV%u - 1;", data->lb_expr, data->depth);
	fprintf(fp, "\\\n"); // new line

	fprintf(fp, "%sub%u = ", data->var_prefix, data->depth);
	fprintf(fp, "DIV%u * %s", data->depth, data->iterator_name);
	fprintf(fp, " + DIV%u - 1", data->depth);
	fprintf(fp, " + %s - DIV%u - 1;", data->lb_expr, data->depth);
	fprintf(fp, "\\\n"); // new line
}