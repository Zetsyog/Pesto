/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#ifndef PESTO_RUNTIME_SCHEDULE_H
#define PESTO_RUNTIME_SCHEDULE_H

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

#define PESTO_RUNTIME_INIT_TILE_PROTO(prefix, ...) void _##prefix##_init_tiles(tile_t(*tileArray)[DIV1], __VA_ARGS__)

#define SCHEDULE_ALLOC(tileArrayName, scheduleName)                                                                    \
	tile_t(*tileArrayName)[DIV1] = NULL;                                                                               \
	schedule_t scheduleName;                                                                                           \
	alloc_tiles(&tileArrayName);                                                                                       \
	alloc_schedule(&scheduleName);

#ifndef SCHED_ALLOC_SIZE
#define SCHED_ALLOC_SIZE min(3000, (DIV0) * (DIV1))
#endif

#define SCHEDULE_COMPUTE(prefix, tiles, schedule, ...)                                                                 \
	do {                                                                                                               \
		_##prefix##_init_tiles(tiles, __VA_ARGS__);                                                                    \
		_pesto_compute_schedule(&schedule, tiles);                                                                     \
	} while (0)

// DIV0 and DIV1 macros should be defined somewhere in the source code
#ifndef DIV0
#define DIV0 8
#endif

#ifndef DIV1
#define DIV1 8
#endif

/* Type declaration */
struct tile {
	long int t0, t1;
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

struct schedule {
	long int len;
	node_t *fronts;
};
typedef struct schedule schedule_t;

/**
 * @brief initialize the tile bounds
 * This function is kernel dependent because of the bounds of the tiles
 * So the user should implement it
 */

/* Alloc/free functions */
void alloc_tiles(tile_t (**tiles)[]) {
	*tiles = calloc(1, sizeof(tile_t[DIV0][DIV1]));
	if (tiles == NULL) {
		fprintf(stderr, "Error: malloc failed tiles (%lu)\n", sizeof(tile_t *) * DIV0);
		exit(1);
	}
}

void alloc_schedule(schedule_t *schedule) {
	schedule->fronts = calloc(SCHED_ALLOC_SIZE, sizeof(node_t));
	if (schedule == NULL) {
		fprintf(stderr, "Error: malloc failed schedule (%lu)\n", sizeof(node_t) * SCHED_ALLOC_SIZE);
		exit(1);
	}
	schedule->len = 0;
}

void free_tiles(tile_t **tiles) {
	free(tiles);
}

void free_schedule(schedule_t *schedule) {
	free(schedule->fronts);
}

void _pesto_compute_schedule(schedule_t *schedule, tile_t (*tileArray)[DIV1]) {
	long int sched_len = 0;

	// On parcourt toutes les tiles de la grille
	for (long int i = 0; i < DIV0; i++) {
		for (long int j = 0; j < DIV1; j++) {
			// Si la tile a déjà été marquée, on passe à la suivante
			// tile marquée = déjà dans le schedule
			if (tileArray[i][j].marked)
				continue;

			// On ajoute la tile courante au schedule
			schedule->fronts[sched_len].tiles[schedule->fronts[sched_len].len] = &tileArray[i][j];
			schedule->fronts[sched_len].len = 1;
			tileArray[i][j].marked = 1;
			// Condition pour éviter la violation de dépendance
			long int last_lb1 = tileArray[i][j].lb1;
			// On parcourt toutes les tiles de i+1 à la fin de la ligne
			// et de 0 à j-1
			for (long int i1 = i + 1; i1 < DIV0; i1++) {
				for (long int j1 = 0; j1 < DIV1; j1++) {
					// Si la tile a déjà été marquée, on passe à la suivante
					if (tileArray[i1][j1].marked) {
						continue;
					}
					// Si la tile courante ne viole pas la dépendance, on l'ajoute au schedule et on passe à la colonne
					// suivante
					if (last_lb1 > tileArray[i1][j1].ub1) {
						schedule->fronts[sched_len].tiles[schedule->fronts[sched_len].len] = &tileArray[i1][j1];
						tileArray[i1][j1].marked = 1;
						schedule->fronts[sched_len].len++;
						// on met à jour la condition pour éviter la violation de dépendance
						last_lb1 = tileArray[i1][j1].lb1;
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

#endif // PESTO_RUNTIME_SCHEDULE_H
