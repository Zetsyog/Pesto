#include "fixture.h"

int main(int argc, char **argv) {
	int n = dimension(argc, argv), seed = atoi(argv[2]);
	int a[CAPACITY][CAPACITY];
	int t, i;
	init_matrix(a, seed);
#pragma scop
	for (t = 1; t < n; ++t)
		for (i = 1; i < n - 1; ++i)
			a[t][i] = (a[t - 1][i - 1] + a[t - 1][i] + a[t - 1][i + 1]) % 97;
#pragma endscop
	dump_matrix(a);
	return 0;
}
