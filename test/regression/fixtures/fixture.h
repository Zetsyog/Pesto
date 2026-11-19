#ifndef PESTO_REGRESSION_FIXTURE_H
#define PESTO_REGRESSION_FIXTURE_H

#include <stdio.h>
#include <stdlib.h>

// Fixed storage keeps zero-sized iteration domains valid C. Dumping all storage
// also checks that transformed loops leave the unused borders untouched.
#define CAPACITY 40

static inline int dimension(int argc, char **argv) {
	if (argc != 3)
		exit(2);
	int n = atoi(argv[1]);
	if (n < 0 || n > 17)
		exit(2);
	return n;
}

static inline void init_matrix(int a[CAPACITY][CAPACITY], int seed) {
	for (int i = 0; i < CAPACITY; ++i)
		for (int j = 0; j < CAPACITY; ++j)
			a[i][j] = (seed + 3 * i + 5 * j) % 17;
}

static inline void dump_matrix(int a[CAPACITY][CAPACITY]) {
	for (int i = 0; i < CAPACITY; ++i) {
		for (int j = 0; j < CAPACITY; ++j)
			printf("%d ", a[i][j]);
		printf("\n");
	}
}

static inline void dump_vector(int a[CAPACITY]) {
	for (int i = 0; i < CAPACITY; ++i)
		printf("%d ", a[i]);
	printf("\n");
}

#endif
