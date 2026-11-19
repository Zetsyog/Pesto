#include <stdio.h>

#ifndef SIZE
#define SIZE 24
#endif

int main(void) {
	int a[SIZE][SIZE];
	int i, j;
	int n = SIZE;

#pragma scop
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			a[i][j] = i * n + j;
		}
	}
#pragma endscop

	// Dump every value, so the test detects reordered or missing writes.
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			printf("%d ", a[i][j]);
		}
		printf("\n");
	}
	return 0;
}
