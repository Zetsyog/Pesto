#include "fixture.h"

int main(int argc, char **argv) {
	int n = dimension(argc, argv), seed = atoi(argv[2]);
	int output[CAPACITY][CAPACITY];
	int i, j;
	init_matrix(output, 11);
#pragma scop
	for (i = 0; i < n; ++i)
		for (j = 0; j < n; ++j) {
			if (i + j < n)
				output[i][j] = seed + i + j;
			else
				output[i][j] = seed - i - j;
		}
#pragma endscop
	dump_matrix(output);
	return 0;
}
