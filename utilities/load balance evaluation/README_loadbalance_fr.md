# Mesure de l'équilibrage de charge

Deux fichiers suffisent : `loadbalance.h`, qui porte la machinerie, et
`instrument_omp.awk`, qui l'injecte dans un code engendré. Le script ne sait
rien du noyau et fonctionne aussi bien sur la sortie de PESTO que sur celle de
Pluto, ce qui rend les deux mesures comparables — c'est le même code qui les
produit.

## Ce que la mesure donne

Deux indicateurs, sommés **région par région** puis rapportés. Un déséquilibre
qui s'inverse d'une région à l'autre se compenserait dans un total global alors
qu'il coûte à chaque barrière ; sommer par région évite cet artefact.

- **volume** — nombre de points d'itération exécutés par chaque thread.
  Exact, reproductible, et directement comparable entre deux versions puisque
  le domaine est le même. C'est l'indicateur à privilégier pour un argument.
- **temps** — durée passée par chaque thread dans la région. Capte ce que le
  volume ne dit pas : localité, effets NUMA, fréquence.

Chacun est le rapport `moyenne / maximum` : **1 signifie l'équilibre parfait**,
et une valeur de 0,5 que le thread le plus chargé fait deux fois le travail
moyen, donc que la moitié de la puissance est perdue aux barrières.

## Utilisation

    awk -f instrument_omp.awk noyau.pesto.c > noyau.pesto.lb.c
    awk -f instrument_omp.awk noyau.pluto.c > noyau.pluto.lb.c

    gcc -O3 -march=native -fopenmp -DPESTO_LOADBALANCE \
        -I. -I<vos includes> noyau.pesto.lb.c -o noyau.pesto.lb -lm

    OMP_NUM_THREADS=64 ./noyau.pesto.lb

La sortie va sur `stderr`, sans perturber un dump de résultats :

    [loadbalance] regions=400  mean_points=4010000  balance_volume=0.4251  balance_time=0.4263

Sans `-DPESTO_LOADBALANCE`, toutes les macros se réduisent à `do {} while (0)` :
le fichier instrumenté est alors strictement équivalent à l'original, et peut
servir de source unique.

## Intégration à une campagne

Les codes Pluto sont régénérés à chaque taille de tuile, donc l'instrumentation
doit être réappliquée à chaque fois. Dans les scripts de recherche, insérer
l'appel entre la génération et la compilation :

    ${POLYCC} --tile --parallel ... ${CODENAME}.c -o ${CODENAME}.pluto.c
    sed -i "s/pragma omp \(.*\)/pragma omp \1 schedule(static)/g" ${CODENAME}.pluto.c
    awk -f instrument_omp.awk ${CODENAME}.pluto.c > ${CODENAME}.pluto.lb.c
    gcc ... -DPESTO_LOADBALANCE ${CODENAME}.pluto.lb.c -o ${CODENAME}.pluto

## Mesurer le temps proprement

Le comptage des points ajoute une incrémentation par itération de la boucle la
plus interne. Pour une mesure de temps non perturbée, recompiler avec

    -DPESTO_LOADBALANCE -DPESTO_LB_TIME_ONLY

qui neutralise le compteur de volume et ne conserve que les chronomètres. Deux
exécutions sont donc nécessaires si les deux indicateurs sont voulus.

## Lecture des résultats

Comparer PESTO en `static` à Pluto en `static` : c'est la comparaison des
équilibres **construits** par les deux pavages, et celle qui porte l'argument.

Comparer Pluto en `static` à Pluto en `dynamic` ou `guided` : cela mesure ce que
l'ordonnanceur OpenMP rattrape, et donc ce que le déséquilibre initial coûte
quand on le laisse à l'exécution.

Comparer PESTO en `static` à Pluto en `dynamic` mêle les deux effets et se prête
mal à une conclusion.

## Ce sur quoi le script s'appuie

Trois régularités des deux générateurs, à vérifier si un jour leur sortie change :

- une région parallèle est un `#pragma omp parallel for`, éventuellement continué
  sur plusieurs lignes par `\`, suivi de la boucle qu'il distribue ;
- un point d'itération est une entrée dans le corps d'une boucle qui n'en
  contient aucune autre — le fichier est lu en entier et chaque `for` apparié à
  son accolade fermante, ce qui évite de compter aussi les niveaux
  intermédiaires ;
- les variables de mesure sont déclarées dans le corps de la boucle parallèle,
  donc privées d'office : les clauses `private` et `firstprivate` existantes ne
  sont pas touchées.

Le nombre de threads est relevé à l'exécution par `omp_get_num_threads()`, ce
qui couvre la clause `num_threads()` que PESTO émet et reste juste lorsqu'un
thread ne reçoit aucune itération sous `dynamic`.

## Limites connues

- Le travail effectué **hors** région parallèle n'est pas compté : l'indicateur
  porte sur la répartition entre threads, non sur la fraction séquentielle.
- Le report final est placé avant le premier `return` de `main`. Si le noyau a
  une autre structure, le script le signale par un commentaire en fin de fichier
  et l'appel à `__lb_report()` est à poser à la main.
- Une région dont toutes les bornes sont vides est ignorée : elle ne dit rien de
  l'équilibrage et fausserait la moyenne des régions.
