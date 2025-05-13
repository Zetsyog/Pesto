#include <assert.h>
#include <stdlib.h>

#include <osl/osl.h>

#include <pesto/osl_support.h>

osl_vector_p osl_relation_row_as_vector(osl_relation_p rel, int row) {
	assert(row < rel->nb_rows);
	int precision = rel->precision;
	osl_vector_p vec = osl_vector_pmalloc(precision, rel->nb_columns);

	for (int i = 0; i < rel->nb_columns; i += 1) {
		osl_int_assign(precision, &vec->v[i], rel->m[row][i]);
	}
	return vec;
}

osl_vector_p osl_statement_get_scattering_first_dim_vector(osl_statement_p statement) {
	assert(statement != NULL);
	osl_relation_p scattering = statement->scattering;
	osl_int_t mone;
	osl_int_init_set_si(scattering->precision, &mone, -1);
	for (int i = 0; i < scattering->nb_rows; i += 1) {
		if (osl_int_eq(scattering->precision, scattering->m[i][1], mone)) {
			return osl_relation_row_as_vector(scattering, i);
		}
	}
	return NULL;
}

/*
 * Count the number of loop nests in the scop
 * We have multiple loop nests if the first scattering dimension is scalar
 * So first we check that all stmts have scalar first scat dimension
 * Then we compute the maximum value of the first scattering dimension
 * Finally we count the number of unique values of the first scattering
 * dimension that have at least one statement
 *
 */
int osl_scop_count_loop_nests(osl_scop_p scop) {
	// ensure that the scop is not NULL
	assert(scop != NULL);
	// check that there is at least one statement
	assert(scop->statement != NULL);

	int precision = scop->statement->scattering->precision;

	// first check if the first dimension is scalar
	for (osl_statement_p stmt = scop->statement; stmt != NULL; stmt = stmt->next) {
		osl_vector_p first_dim = osl_statement_get_scattering_first_dim_vector(stmt);
		assert(first_dim != NULL);

		osl_int_set_si(first_dim->precision, &first_dim->v[1], 0);

		// check if the first dimension is scalar
		int is_scalar = osl_vector_is_scalar(first_dim);
		osl_vector_free(first_dim);

		if (!is_scalar) {
			return 1;
		}
	}

	// At this point we know that the first scaterring dimension is scalar
	// so we have multiple loop nests

	// Compute the maximum value of the first scattering dimension
	osl_int_t max_scalar_value;
	osl_int_init_set_si(precision, &max_scalar_value, 0);

	for (osl_statement_p stmt = scop->statement; stmt != NULL; stmt = stmt->next) {
		if (osl_int_gt(precision, stmt->scattering->m[0][stmt->scattering->nb_columns - 1], max_scalar_value)) {
			osl_int_assign(precision, &max_scalar_value, stmt->scattering->m[0][stmt->scattering->nb_columns - 1]);
		}
	}

	int si_max = osl_int_get_si(precision, max_scalar_value);
	int *stmt_per_nest = (int *)calloc(si_max + 1, sizeof(int));
	int count = 0;
	for (osl_statement_p stmt = scop->statement; stmt != NULL; stmt = stmt->next) {
		int scalar_value = osl_int_get_si(precision, stmt->scattering->m[0][stmt->scattering->nb_columns - 1]);
		if (stmt_per_nest[scalar_value] == 0) {
			count += 1;
		}
		stmt_per_nest[scalar_value] += 1;
	}

	free(stmt_per_nest);
	return count;
}

osl_scop_p *osl_scop_split_nests(osl_scop_p scop) {
	assert(scop != NULL);
	int nest_count = osl_scop_count_loop_nests(scop);
	if (nest_count <= 1) {
		return NULL;
	}
	osl_scop_p *new_scops = (osl_scop_p *)malloc(nest_count * sizeof(osl_scop_p));

	// create a new scop for each loop nest
	for (int i = 0; i < nest_count; i++) {
		new_scops[i] = osl_scop_malloc();
		new_scops[i]->version = scop->version;
		if (scop->language != NULL)
			OSL_strdup(new_scops[i]->language, scop->language);

		new_scops[i]->context = osl_relation_clone(scop->context);
		new_scops[i]->parameters = osl_generic_clone(scop->parameters);
		new_scops[i]->registry = osl_interface_clone(scop->registry);
		new_scops[i]->extension = osl_generic_clone(scop->extension);

		if (scop->next != NULL) {
			fprintf(stderr, "WARNING: Chained scops are not supported\n");
		}
		// TODO: support of chained scops
	}

	osl_int_t scat_value;
	osl_int_init_set_si(scop->statement->scattering->precision, &scat_value, 0);
	// now we need to add the corresponding statements in each scop
	// for each nest
	for (int i = 0; i < nest_count; i++) {
		// iterate through all statements and add the ones that belong to the
		// nest
		// scalar value of the first scattering dimension could be anything so
		// we have to check all values
		int found_stmt = 0;
		while (found_stmt == 0) {
			for (osl_statement_p stmt = scop->statement; stmt != NULL; stmt = stmt->next) {
				osl_vector_p first_dim = osl_statement_get_scattering_first_dim_vector(stmt);

				if (osl_int_eq(stmt->scattering->precision, first_dim->v[stmt->scattering->nb_columns - 1],
							   scat_value)) {
					osl_statement_add(&new_scops[i]->statement, osl_statement_nclone(stmt, 1));
					found_stmt += 1;
				}
				osl_vector_free(first_dim);
			}
			osl_int_add_si(scop->statement->scattering->precision, &scat_value, scat_value, 1);
		}
	}
	return new_scops;
}

int osl_max_scattering_dim(osl_scop_p scop) {
	assert(scop != NULL);
	int max_dim = 0;
	for (osl_statement_p stmt = scop->statement; stmt != NULL; stmt = stmt->next) {
		if (stmt->scattering->nb_output_dims > max_dim) {
			max_dim = stmt->scattering->nb_output_dims;
		}
	}
	return max_dim;
}

int osl_relation_is_dim_scalar(osl_relation_p rel, int dim) {
	assert(dim < rel->nb_output_dims);
	int precision = rel->precision;
	osl_int_t mone;
	osl_int_init_set_si(precision, &mone, -1);
	for (int i = 0; i < rel->nb_rows; i += 1) {
		// if it is an equality
		if (osl_int_zero(precision, rel->m[i][0])) {
			// ensure that all other columns are zero except the dim
			for (int j = 1; j < rel->nb_columns - 1; j += 1) {
				if (j == dim) {
					continue;
				}
				if (!osl_int_zero(precision, rel->m[i][j])) {
					return 0;
				}
			}
			if (!osl_int_zero(precision, rel->m[i][dim])) {
				return 1;
			}
		}
	}
	return 0;
}

bool osl_relation_is_row_equality(osl_relation_p rel, int row) {
	assert(rel != NULL);
	assert(row >= 0 && row < rel->nb_rows);
	return osl_int_zero(rel->precision, rel->m[row][0]);
}

bool osl_relation_is_row_inequality(osl_relation_p rel, int row) {
	assert(rel != NULL);
	assert(row >= 0 && row < rel->nb_rows);
	return !osl_int_zero(rel->precision, rel->m[row][0]);
}

bool osl_relation_is_tiling_dim(osl_relation_p relation, int dim) {
	assert(relation != NULL);
	assert(relation->type == OSL_TYPE_DOMAIN);
	assert(dim >= 0 && dim < relation->nb_output_dims);

	int dimIdx = dim + 1;

	for (int i = 0; i < relation->nb_rows; i += 1) {
		if (osl_relation_is_row_equality(relation, i)) {
			continue;
		}
		if (osl_int_zero(relation->precision, relation->m[i][dimIdx])) {
			continue;
		}
		osl_vector_p ivec = osl_relation_row_as_vector(relation, i);
		osl_int_set_si(ivec->precision, &(ivec->v[0]), 0);

		// relation must involve at least two iterators
		int involved_iterators = 0;
		for (int j = 0; j < relation->nb_output_dims; j += 1) {
			if (!osl_int_zero(relation->precision, ivec->v[1 + j])) {
				involved_iterators += 1;
			}
		}
		if (involved_iterators < 2) {
			osl_vector_free(ivec);
			continue;
		}

		for (int j = 0; j < relation->nb_rows; j += 1) {
			if (i == j) {
				continue;
			}
			osl_vector_p jvec = osl_relation_row_as_vector(relation, j);
			osl_int_set_si(jvec->precision, &(jvec->v[0]), 0);
			osl_vector_p sum = osl_vector_add(ivec, jvec);
			// ignore scalar
			osl_int_set_si(sum->precision, &(sum->v[sum->size - 1]), 0);
			if (!osl_vector_is_scalar(sum)) {
				osl_vector_free(sum);
				osl_vector_free(jvec);
				continue;
			}
			osl_int_t tilingFactor;
			osl_int_t paddedTilingFactor;
			osl_int_init(relation->precision, &tilingFactor);
			osl_int_init(relation->precision, &paddedTilingFactor);
			osl_int_abs(relation->precision, &tilingFactor, ivec->v[dimIdx]);
			osl_int_add_si(relation->precision, &paddedTilingFactor, tilingFactor, -1);

			if (!osl_int_zero(relation->precision, paddedTilingFactor)) {
				if (osl_int_eq(relation->precision, paddedTilingFactor, relation->m[j][relation->nb_columns - 1])) {
					osl_int_clear(relation->precision, &tilingFactor);
					osl_int_clear(relation->precision, &paddedTilingFactor);
					osl_vector_free(sum);
					osl_vector_free(jvec);
					osl_vector_free(ivec);
					return true;
				}
			}

			osl_int_clear(relation->precision, &tilingFactor);
			osl_int_clear(relation->precision, &paddedTilingFactor);

			osl_vector_free(sum);
			osl_vector_free(jvec);
		}

		osl_vector_free(ivec);
	}

	return false;
}
