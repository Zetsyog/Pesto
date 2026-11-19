#include "fixture.h"

int main(int argc, char **argv) {
	int n = dimension(argc, argv), seed = atoi(argv[2]);
	int a[CAPACITY][CAPACITY], b[CAPACITY][CAPACITY];
	int i, j;
	init_matrix(a, 11);
	init_matrix(b, 13);
#pragma scop
	for (i = 0; i < n; ++i)
		for (j = 0; j < n; ++j) {
			a[i][j] = seed + 3 * i + j;
			b[i][j] = 2 * a[i][j] - i;
		}
	for (i = 0; i < n; ++i)
		for (j = 0; j < n; ++j)
			a[i][j] += b[j][i];
#pragma endscop
	dump_matrix(a);
	dump_matrix(b);
	return 0;
}
