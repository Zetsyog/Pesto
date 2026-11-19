#include "fixture.h"

int main(int argc, char **argv) {
	int n = dimension(argc, argv), seed = atoi(argv[2]);
	int input[CAPACITY][CAPACITY], output[CAPACITY];
	int i, k;
	init_matrix(input, seed);
	for (i = 0; i < CAPACITY; ++i)
		output[i] = -99;
#pragma scop
	for (i = 0; i < n; ++i) {
		output[i] = seed;
		for (k = 0; k < n; ++k)
			output[i] += input[i][k];
		output[i] = 2 * output[i] + i;
	}
#pragma endscop
	dump_matrix(input);
	dump_vector(output);
	return 0;
}
