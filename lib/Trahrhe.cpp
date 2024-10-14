#include <pesto/Pesto.hpp>

std::string pesto::trahrhe::ScheduleHeader = R"(
#ifndef LAB_SCHEDULE_H
#define LAB_SCHEDULE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* MACRO DEFINITIONS */
#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif
#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef SCHED_ALLOC_SIZE
#define SCHED_ALLOC_SIZE min(3000, (DIV0) * (DIV1))
#endif

#define SCHEDULE_ALLOC(tiles, schedule)                                                                                \
	do {                                                                                                               \
		alloc_tiles(&tiles);                                                                                           \
		alloc_schedule(&schedule);                                                                                     \
	} while (0)

#define SCHEDULE_FREE(tiles, schedule)                                                                                 \
	do {                                                                                                               \
		free_tiles(tiles);                                                                                             \
		free_schedule(&schedule);                                                                                      \
	} while (0)

#ifdef SCHEDULE_OPT1
#define _SCHEDULE_RUN_OPT()                                                                                            \
	do {                                                                                                               \
		tile_opt_pass1(tiles);                                                                                         \
	} while (0)
#else
#define _SCHEDULE_RUN_OPT()                                                                                            \
	do {                                                                                                               \
	} while (0)
#endif

#ifdef METRICS_SCHEDULE_COMPUTE_TIME
#define SCHEDULE_COMPUTE(tiles, schedule, ...)                                                                         \
	do {                                                                                                               \
		polybench_start_instruments;                                                                                   \
		init_tiles(tiles, __VA_ARGS__);                                                                                \
		_SCHEDULE_RUN_OPT();                                                                                           \
		compute_schedule(&schedule);                                                                                   \
		/* Stop and print timer. */                                                                                    \
		polybench_stop_instruments;                                                                                    \
		polybench_print_instruments;                                                                                   \
	} while (0)
#else
#define SCHEDULE_COMPUTE(tiles, schedule, ...)                                                                         \
	do {                                                                                                               \
		init_tiles(tiles, __VA_ARGS__);                                                                                \
		_SCHEDULE_RUN_OPT();                                                                                           \
		compute_schedule(&schedule);                                                                                   \
	} while (0)
#endif

#define INIT_TILES_PROTO(...) void init_tiles(tile_t **tiles, __VA_ARGS__)

// DIV0 and DIV1 macros should be defined somewhere in the source code
#ifndef DIV0
#define DIV0 8
#endif

#ifndef DIV1
#define DIV1 8
#endif

/* Type declaration */
struct tile {
	long int lb0, ub0;
	long int lb1, ub1;
	char marked;
};
typedef struct tile tile_t;

struct node {
	long int len;
	tile_t *tiles[DIV1 * DIV0];
};
typedef struct node node_t;

/* Global variables */
tile_t **tiles; // 2D array of tiles
struct schedule {
	long int len;
	node_t *fronts;
};
typedef struct schedule schedule_t;

schedule_t schedule;

/**
 * @brief initialize the tile bounds
 * This function is kernel dependent because of the bounds of the tiles
 * So the user should implement it
 */
// extern void init_tiles();

/**
 * @brief initialize the schedule
 * Build a schedule of tiles to allow parallel execution
 * Algorithm:
 * Iteration domain should have been tiled before and all the tile bounds are stored in the tiles global variable
 * We iterate over the tiles array in the lexicographic order
 * If the current tile t is not marked, we create a new node in the schedule (new parallel front) and add t
 * We initialize the variable last_lb1 to lb1 of t
 * Then for each tile t' that is after t in the lexicographic order
 * If t' is not marked and lb1(t') < last_lb1,
 * we add t' to the current node in the schedule and update last_lb1 to lb1(t')
 */
void compute_schedule(schedule_t *schedule) {
	long int sched_len = 0;

	// On parcourt toutes les tiles de la grille
	for (long int i = 0; i < DIV0; i++) {
		for (long int j = 0; j < DIV1; j++) {
			// Si la tile a déjà été marquée, on passe à la suivante
			// tile marquée = déjà dans le schedule
			if (tiles[i][j].marked)
				continue;

			// On ajoute la tile courante au schedule
			schedule->fronts[sched_len].tiles[schedule->fronts[sched_len].len] = &tiles[i][j];
			schedule->fronts[sched_len].len = 1;
			tiles[i][j].marked = 1;
			// Condition pour éviter la violation de dépendance
			long int last_lb1 = tiles[i][j].lb1;
			// On parcourt toutes les tiles de i+1 à la fin de la ligne
			// et de 0 à j-1
			for (long int i1 = i + 1; i1 < DIV0; i1++) {
				for (long int j1 = 0; j1 < DIV1; j1++) {
					// Si la tile a déjà été marquée, on passe à la suivante
					if (tiles[i1][j1].marked) {
						continue;
					}
					// Si la tile courante ne viole pas la dépendance, on l'ajoute au schedule et on passe à la colonne
					// suivante
					if (last_lb1 > tiles[i1][j1].ub1) {
						schedule->fronts[sched_len].tiles[schedule->fronts[sched_len].len] = &tiles[i1][j1];
						tiles[i1][j1].marked = 1;
						schedule->fronts[sched_len].len++;
						// on met à jour la condition pour éviter la violation de dépendance
						last_lb1 = tiles[i1][j1].lb1;
						break;
					}
				}
			}
			// front parallèle suivant
			sched_len++;
		}
	}
	schedule->len = sched_len;
}

/* Alloc/free functions */
void alloc_tiles(tile_t ***tiles) {
	*tiles = malloc(sizeof(tile_t *) * DIV0);
	if (tiles == NULL) {
		fprintf(stderr, "Error: malloc failed tiles (%lu)\n", sizeof(tile_t *) * DIV0);
		exit(1);
	}
	for (long int i = 0; i < DIV0; i++) {
		(*tiles)[i] = malloc(sizeof(tile_t) * DIV1);
		if ((*tiles)[i] == NULL) {
			fprintf(stderr, "Error: malloc failed tiles[%ld] (%lu)\n", i, sizeof(tile_t) * DIV1);
			exit(1);
		}
	}
}

void alloc_schedule(schedule_t *schedule) {
	schedule->fronts = malloc(SCHED_ALLOC_SIZE * sizeof(node_t));
	if (schedule == NULL) {
		fprintf(stderr, "Error: malloc failed schedule (%lu)\n", sizeof(node_t) * SCHED_ALLOC_SIZE);
		exit(1);
	}
	schedule->len = 0;
}

void free_tiles(tile_t **tiles) {
	for (long int i = 0; i < DIV0; i++) {
		free(tiles[i]);
	}
	free(tiles);
}

void free_schedule(schedule_t *schedule) { free(schedule->fronts); }

void dump_schedule(FILE *fp, schedule_t *schedule) {
	fprintf(fp, "front,it,lb0,ub0,lb1,ub1\n");
	for (long int i = 0; i < schedule->len; i++) {
		for (long int j = 0; j < schedule->fronts[i].len; j++) {
			fprintf(fp, "%ld,%ld,%ld,%ld,%ld,%ld\n", i, j, schedule->fronts[i].tiles[j]->lb0,
					schedule->fronts[i].tiles[j]->ub0, schedule->fronts[i].tiles[j]->lb1,
					schedule->fronts[i].tiles[j]->ub1);
		}
	}
	fprintf(fp, "\n");
}

#ifndef SCHEDULE_OPT1_TOLERANCE
#define SCHEDULE_OPT1_TOLERANCE 5
#endif

void tile_opt_pass1(tile_t **tiles) {
	for (long int i = 0; i < DIV0; i++) {
		for (long int j = 0; j < DIV1; j++) {
			tiles[i][j].marked = 0;
		}
	}

#ifdef SCHEDULE_OPT1_HEAP_ALLOC
	struct {
		int x, y;
	} *t = calloc(DIV0, sizeof(struct { int x, y; }));
#else
	struct {
		int x, y;
	} t[DIV0] = {0};
#endif
	// iterate over the tiles in the lexicographic order
	for (int i = 0; i < DIV0; i++) {
		for (int j = 0; j < DIV1; j++) {
			for (int k = 0; k < DIV0; k++) {
				t[k].x = 0;
				t[k].y = 0;
			}
			int next_idx = 0;

			for (int i2 = i + 1; i2 < DIV0; i2 += 1) {
				for (int j2 = 0; j2 < DIV1; j2 += 1) {
					if (tiles[i2][j2].marked)
						continue;
					if (tiles[i2][j2].lb1 >= tiles[i][j].lb1 + SCHEDULE_OPT1_TOLERANCE &&
						tiles[i2][j2].lb1 <= tiles[i][j].lb1 - SCHEDULE_OPT1_TOLERANCE) {
						tiles[i2][j2].marked = 1;
						t[next_idx].x = i2;
						t[next_idx++].y = j2;
						break;
					}
				}
			}
			long int mean = 0;
			for (int k = 0; k < next_idx; k++) {
				mean += tiles[t[k].x][t[k].y].lb1;
			}
			mean /= next_idx;

			for (int k = 0; k < next_idx; k++) {
				tiles[t[k].x][t[k].y].lb1 = mean;
				if (t[k].y > 0) {
					tiles[t[k].x][t[k].y - 1].ub1 = mean - 1;
				}
			}
		}
	}

	for (long int i = 0; i < DIV0; i++) {
		for (long int j = 0; j < DIV1; j++) {
			tiles[i][j].marked = 0;
		}
	}
#ifdef SCHEDULE_OPT1_HEAP_ALLOC
	free(t);
#endif
}

#endif // LAB_SCHEDULE_H

)";
