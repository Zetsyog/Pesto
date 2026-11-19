#include "fixture.h"

int main(int argc, char **argv) {
	int n = dimension(argc, argv), seed = atoi(argv[2]);
	int input[CAPACITY][CAPACITY], output[CAPACITY];
	int i, j, temp;
	init_matrix(input, seed);
	for (i = 0; i < CAPACITY; ++i)
		output[i] = -99;
#pragma scop
	for (i = 0; i < n; ++i) {
		temp = seed + i;
		for (j = 0; j < n; ++j)
			temp += input[i][j];
		output[i] = temp;
	}
#pragma endscop
	dump_matrix(input);
	dump_vector(output);
	return 0;
}
