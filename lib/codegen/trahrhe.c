#include <stdlib.h>
#include <string.h>

#include <pesto/trahrhe.h>

void trahrhe_codegen_header_stmt_algebraic(FILE *fp,
										   struct trahrhe_codegen_data *data) {
	/*    Tiling upper bound computation     aub0t = DIV0 - 1;   */
	fprintf(fp, "b%d_ubt%u = DIV%u - 1; ", data->band_id, data->depth,
			data->depth);
	fprintf(fp, "\\\n"); // new line

	fprintf(fp, "b%u_t%d_pcmax = ", data->band_id, data->depth);
	fprintf(fp, "b%u_t%d_Ehrhart(", data->band_id, data->depth);
	for (int i = 0; i < data->npar; i++) {
		fprintf(fp, "%s", data->params[i]);
		if (i != data->npar - 1) {
			fprintf(fp, ", ");
		}
	}
	for (int i = 0; i < data->depth; i++) {
		fprintf(fp, ", b%d_lb%d, b%d_ub%d", data->band_id, i, data->band_id, i);
	}
	fprintf(fp, ");");
	fprintf(fp, "\\\n"); // new line
	fprintf(fp, "b%u_TILE_VOL_L%d = ", data->band_id, data->depth);
	fprintf(fp, "b%u_t%d_pcmax / DIV%u ", data->band_id, data->depth,
			data->depth);
}

void trahrhe_codegen_header_stmt(FILE *fp, struct trahrhe_codegen_data *data) {
	switch (data->tiling_type) {
	case algebraic:
		trahrhe_codegen_header_stmt_algebraic(fp, data);
		break;
	case algebraic_standalone:
		// trahrhe_codegen_header_stmt_algebraic_stdln(fp, data);
		break;
	case rectangular:
		// trahrhe_codegen_header_stmt_rectangular(fp, data);
		break;
	default:
		fprintf(stderr, "Not implemented yet\n");
		exit(1);
	}
}

void trahrhe_codegen_body_stmt(FILE *fp, struct trahrhe_codegen_data *data) {
	switch (data->tiling_type) {
	case algebraic:
		// trahrhe_codegen_body_stmt_algebraic(fp, data);
		break;
	case algebraic_standalone:
		// trahrhe_codegen_body_stmt_algebraic_stdln(fp, data);
		break;
	case rectangular:
		// trahrhe_codegen_body_stmt_rectangular(fp, data);
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
