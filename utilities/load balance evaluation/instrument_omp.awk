#!/usr/bin/awk -f
#
# instrument_omp.awk — injecte l'instrumentation d'equilibrage de charge dans un
# code engendre par PESTO ou par Pluto, sans rien savoir du noyau. A relancer
# apres chaque generation, les diviseurs et tailles de tuiles changeant le code.
#
#   awk -f instrument_omp.awk kernel.pesto.c > kernel.pesto.lb.c
#   awk -f instrument_omp.awk kernel.pluto.c > kernel.pluto.lb.c
#
# Ce sur quoi il s'appuie, et qui vaut pour les deux generateurs :
#   - une region parallele est un "#pragma omp parallel for", eventuellement
#     continue sur plusieurs lignes par "\", suivi de la boucle qu'il distribue ;
#   - un point d'iteration est une entree dans le corps d'une boucle qui n'en
#     contient aucune autre : le fichier est lu en entier, puis chaque "for" est
#     apparie a son accolade fermante pour savoir s'il est le plus interne ;
#   - les variables de mesure sont declarees dans le corps de la boucle
#     parallele, donc privees d'office : les clauses existantes sont intactes ;
#   - le nombre de threads est deduit a l'execution, ce qui couvre la clause
#     num_threads() que PESTO emet.

{ line[NR] = $0 }

END {
    n = NR;

    # --- reperer les boucles les plus internes, dans une region seulement --
    for (i = 1; i <= n; i++) {
        if (line[i] !~ /#pragma[ \t]+omp[ \t]+parallel[ \t]+for/) continue;
        pragma_at[i] = 1;
        j = i;
        while (j <= n && line[j] ~ /\\[ \t]*$/) j++;   # continuations
        j++;                                          # la boucle distribuee
        while (j <= n && line[j] !~ /for[ \t]*\(/) j++;
        loop_at[j] = 1;
        d = gsub_count(line[j], "{") - gsub_count(line[j], "}");
        k = j + 1;
        while (k <= n && d > 0) {
            d += gsub_count(line[k], "{") - gsub_count(line[k], "}");
            if (d <= 0) break;
            k++;
        }
        close_at[k] = 1;
        for (m = j; m <= k; m++) in_par[m] = 1;
    }

    # --- reperer les regions paralleles d'abord ---------------------------
    for (i = 1; i <= n; i++) {
        if (line[i] !~ /^[ \t]*for[ \t]*\(/) continue;
        if (line[i] !~ /\{[ \t]*$/) continue;      # corps sur une seule ligne : ignore
        d = 1; j = i + 1; has_inner = 0;
        while (j <= n && d > 0) {
            if (line[j] ~ /^[ \t]*for[ \t]*\(/) has_inner = 1;
            d += gsub_count(line[j], "{") - gsub_count(line[j], "}");
            j++;
        }
        if (!has_inner && in_par[i]) innermost[i] = 1;
    }

    # --- emettre -----------------------------------------------------------
    printf("#include \"loadbalance.h\"\n");
    for (i = 1; i <= n; i++) {
        if (pragma_at[i]) print "LB_BEGIN();";
        if (close_at[i])  print "LB_ACC();";
        if (line[i] ~ /int[ \t]+main[ \t]*\(/) in_main = 1;
        if (!done_report && in_main && line[i] ~ /^[ \t]*return[ \t]/) {
            print "LB_REPORT();"; done_report = 1;
        }
        print line[i];
        if (loop_at[i])   print "LB_DECL();";
        if (innermost[i]) print "LB_ADD(1);";
        if (close_at[i])  print "LB_END();";
    }
    if (!done_report)
        printf("/* LB_REPORT() non place : appeler __lb_report() a la main */\n");
}

function gsub_count(s, ch,   t, c) { t = s; c = gsub(ch, ch, t); return c }
