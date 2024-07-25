#ifndef PESTO_OSL_SUPPORT_H
#define PESTO_OSL_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <osl/osl.h>

/**
 * @brief create a vector from the given row of relation
 * The vector will have the same precision as the relation
 * The caller should ensure that the row is within the bounds of the relation
 * Newly created vector should be freed by the caller
 *
 * @param rel relation to extract the row from
 * @param row of the relation to extract
 * @return osl_vector_p
 */
osl_vector_p osl_relation_row_as_vector(osl_relation_p rel, int row);

/**
 * @brief count the number of loop nests in the scop
 *
 * @param scop
 * @return int
 */
int osl_scop_count_loop_nests(osl_scop_p scop);

/**
 * @brief get first dimension of the scattering function of the statement as a
 * vector
 * The caller should ensure that the statement is not NULL
 * The caller should ensure that the statement has a scattering function
 * The caller should free the returned vector
 * @param statement
 * @return osl_vector_p
 */
osl_vector_p
osl_statement_get_scattering_first_dim_vector(osl_statement_p statement);

/**
 * @brief split the nests of the scop
 * Split a scop into multiple scops, each containing a single loop nest
 * The caller should free the returned array of scops and each scop in the array
 * The scops return are ordered in the same order as the loop nests i.e.
 * following the order of the scattering function
 * return NULL if there is only one or less loop nest in the scop
 *
 * @param scop
 * @return osl_scop_p* array of scops
 */
osl_scop_p *osl_scop_split_nests(osl_scop_p scop);

/**
 * @brief get the maximum scattering dimension number of the scop
 * @param scop
 * @return int
 */
int osl_max_scattering_dim(osl_scop_p scop);

#ifdef __cplusplus
}
#endif

#endif // PESTO_OSL_SUPPORT_H