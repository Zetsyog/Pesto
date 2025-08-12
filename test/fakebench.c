#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#ifndef FAKE_TIME_RSEED
#define FAKE_TIME_RSEED (time(NULL) * getpid())
#endif

#ifndef FAKE_TIME_MEAN_VALUE
#define FAKE_TIME_MEAN_VALUE 1.0
#endif

#ifndef FAKE_DUMP_RSEED
#define FAKE_DUMP_RSEED time(NULL)
#endif

#ifndef FAKE_DUMP_SIZE
#define FAKE_DUMP_SIZE 100
#endif

int main() {
	/* Fake benchmark */
	/* this program will execute quickly and mimic polybench program behaviour */

#ifdef POLYBENCH_TIME
	/* print a fake execution time to stdout */
	srand(FAKE_TIME_RSEED);
	printf("%0.6f\n", FAKE_TIME_MEAN_VALUE + ((rand() / (double)RAND_MAX) - 0.5) * 0.5);
#endif

#ifdef POLYBENCH_DUMP_ARRAYS
	/* print a fake array to stderr */
	srand(FAKE_DUMP_RSEED);

	for (int i = 0; i < FAKE_DUMP_SIZE; ++i) {
		for (int j = 0; j < FAKE_DUMP_SIZE; ++j) {
			fprintf(stderr, "%.3f ", ((rand() / (double)RAND_MAX)));
		}
		fprintf(stderr, "\n");
	}

#endif
	return 0;
}