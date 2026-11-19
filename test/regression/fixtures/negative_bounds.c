#include "fixture.h"

int main(int argc, char **argv) {
	int n = dimension(argc, argv), seed = atoi(argv[2]);
	int output[CAPACITY];
	int i;
	for (i = 0; i < CAPACITY; ++i)
		output[i] = -99;
#pragma scop
	for (i = -n; i < n; ++i)
		output[i + 17] = 3 * i + seed;
#pragma endscop
	dump_vector(output);
	return 0;
}
