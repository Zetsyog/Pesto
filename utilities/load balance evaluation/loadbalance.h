/* loadbalance.h — instrumentation d'équilibrage de charge, commune aux codes
 * PESTO et Pluto. Tout est neutralisé sans -DPESTO_LOADBALANCE.
 *
 * Deux indicateurs, sommés région par région : un déséquilibre qui s'inverse
 * d'une région à l'autre se compenserait dans un total global alors qu'il coûte
 * à chaque barrière.
 *   volume : nombre d'instances de statement exécutées par chaque thread.
 *            Exact et comparable entre deux versions, le domaine étant le même.
 *   temps  : durée passée par chaque thread dans la région. Capte ce que le
 *            volume ne dit pas — localité, NUMA, fréquence.
 * Le rapport moyenne/max vaut 1 pour un équilibre parfait.
 *
 * Le comptage du volume ajoute une incrémentation par point ; pour une mesure
 * de temps propre, compiler une seconde fois avec -DPESTO_LB_TIME_ONLY.
 */
#ifndef PESTO_LOADBALANCE_H
#define PESTO_LOADBALANCE_H

#ifdef PESTO_LOADBALANCE
#include <stdio.h>
#include <omp.h>

#define __LB_MAXTH 1024
static long   __lb_work[__LB_MAXTH];
static double __lb_time[__LB_MAXTH];
static long   __lb_wmax_sum = 0, __lb_wavg_sum = 0;
static double __lb_tmax_sum = 0.0, __lb_tavg_sum = 0.0;
static int    __lb_nreg = 0;
/* Taille de l'equipe, relevee par tous les threads (meme valeur, course
 * benigne) : la clause num_threads() de PESTO peut la reduire bien en dessous
 * d'OMP_NUM_THREADS, et le total du travail vaut mean_points x team. */
static int    __lb_team = 1;
/* La taille d'equipe varie d'une region a l'autre : num_threads() suit le
 * nombre de tranches, qui depend du volume disponible dans la bande. Une
 * valeur unique serait celle de la derniere region seulement, d'ou l'etendue
 * et le cumul du travail reel plutot qu'un produit mean x team. */
static long   __lb_wtot_sum = 0;
static int    __lb_team_min = 1 << 30, __lb_team_max = 0;
/* Cumuls separes selon la parite de la region : les deux fronts du trapeze
 * alternent, et le decoupage a volume egal equilibre leur SOMME, pas chacun.
 * Deux fronts chacun desequilibre peuvent avoir un total equilibre. */
static long   __lb_wmax_f[2] = {0, 0}, __lb_wavg_f[2] = {0, 0};
static int    __lb_nreg_f[2] = {0, 0};

static void __lb_begin(void) {
	int k;
	for (k = 0; k < __LB_MAXTH; k++) { __lb_work[k] = 0; __lb_time[k] = 0.0; }
}
/* Le nombre de threads est deduit des entrees touchees : PESTO pose une clause
 * num_threads() qui peut etre inferieure a omp_get_max_threads(). */
static void __lb_end(void) {
	long wmax = 0, wsum = 0; double tmax = 0.0, tsum = 0.0; int k;
	int nth = __lb_team > 0 ? __lb_team : 1;
	if (nth > __LB_MAXTH) nth = __LB_MAXTH;
	for (k = 0; k < nth; k++) {
		if (__lb_work[k] > wmax) wmax = __lb_work[k];
		wsum += __lb_work[k];
		if (__lb_time[k] > tmax) tmax = __lb_time[k];
		tsum += __lb_time[k];
	}
	/* Une région sans travail (bornes vides) ne dit rien de l'équilibrage. */
	if (wmax == 0 && tmax == 0.0) return;
	__lb_wtot_sum += wsum;
	if (nth < __lb_team_min) __lb_team_min = nth;
	if (nth > __lb_team_max) __lb_team_max = nth;
	{ int f = __lb_nreg & 1;
	  __lb_wmax_f[f] += wmax; __lb_wavg_f[f] += wsum / nth; __lb_nreg_f[f]++; }
	__lb_wmax_sum += wmax; __lb_wavg_sum += wsum / nth;
	__lb_tmax_sum += tmax; __lb_tavg_sum += tsum / (double)nth;
	__lb_nreg++;
}
static void __lb_report(void) {
	fprintf(stderr,
			"[loadbalance] regions=%d  team=%d..%d  mean_points=%ld  total_points=%ld  "
			"balance_volume=%.4f  balance_time=%.4f\n"
			"[loadbalance] front0: regions=%d balance=%.4f   "
			"front1: regions=%d balance=%.4f\n",
			__lb_nreg, __lb_team_max ? __lb_team_min : 0, __lb_team_max,
			__lb_wavg_sum, __lb_wtot_sum,
			__lb_wmax_sum ? (double)__lb_wavg_sum / (double)__lb_wmax_sum : 1.0,
			__lb_tmax_sum > 0.0 ? __lb_tavg_sum / __lb_tmax_sum : 1.0,
			__lb_nreg_f[0], __lb_wmax_f[0] ? (double)__lb_wavg_f[0] / (double)__lb_wmax_f[0] : 1.0,
			__lb_nreg_f[1], __lb_wmax_f[1] ? (double)__lb_wavg_f[1] / (double)__lb_wmax_f[1] : 1.0);
}

#define LB_BEGIN()  __lb_begin()
#define LB_END()    __lb_end()
#define LB_REPORT() __lb_report()
#define LB_DECL()   int __lb_me = omp_get_thread_num();                        \
                    double __lb_t0 = omp_get_wtime();                          \
                    __lb_team = omp_get_num_threads()
#define LB_ACC()    __lb_time[__lb_me] += omp_get_wtime() - __lb_t0
#ifdef PESTO_LB_TIME_ONLY
#define LB_ADD(n)   do {} while (0)
#else
#define LB_ADD(n)   do { long __n = (long)(n); if (__n > 0) __lb_work[__lb_me] += __n; } while (0)
#endif

#else /* instrumentation désactivée */

#define LB_BEGIN()  do {} while (0)
#define LB_END()     do {} while (0)
#define LB_REPORT() do {} while (0)
#define LB_DECL()   do {} while (0)
#define LB_ACC()    do {} while (0)
#define LB_ADD(n)   do {} while (0)

#endif
#endif /* PESTO_LOADBALANCE_H */
