#include "fixture.h"

int main(int argc, char **argv) {
	int n = dimension(argc, argv), seed = atoi(argv[2]);
	int input[CAPACITY][CAPACITY], output[CAPACITY][CAPACITY];
	int i, j;
	init_matrix(input, seed);
	init_matrix(output, 11);
#pragma scop
	for (i = 1; i <= n; ++i)
		for (j = 1; j <= n; ++j)
			output[i][j] = input[i - 1][j - 1] + seed;
#pragma endscop
	dump_matrix(input);
	dump_matrix(output);
	return 0;
}
