#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_maxcut.py
=============

Genere une fonction C qui calcule, pour chaque tranche d'un nid de boucles
polyedrique, une BORNE SUPERIEURE du volume des coupes selon x2 de hauteur t.

IMPORTANT : le resultat est une borne SUPERIEURE (via l'operateur 'ub' de
barvinok), pas le maximum entier exact. 'ub' majore le quasi-polynome de
volume sur la position de coupe c ; la valeur retournee est donc toujours
>= au vrai maximum (jamais une sous-estimation), mais peut le depasser
legerement. C'est le comportement voulu pour du dimensionnement sur (taille
de buffer, allocation, estimation de charge) ou une majoration est sure.
Pour le maximum EXACT, il faudrait enumerer c a l'execution (boucle en C)
plutot que d'utiliser 'ub'.

Modele geometrique
-------------------
  * Domaine : polytope parametre D[q1..qp] -> { [x1,...,xN] : contraintes }.
  * Tranches selon x1, de largeur t, qui se succedent SANS recouvrement
    a partir de la valeur minimale de x1 :
        tranche b couvre   f1(q) + b*t <= x1 < f1(q) + (b+1)*t
    ou f1(q) = min de x1 sur le domaine (lexmin).
  * Coupes selon x2, de hauteur t, GLISSANTES (toutes positions possibles) :
        coupe a la position c couvre   c <= x2 < c + t
  * t est CONNU A LA COMPILATION (constante entiere).

Pour chaque tranche b on borne :  max_c card( D inter tranche_b inter coupe_c )

Comme t est constant, b*t et c*t sont affines : le probleme reste dans
l'arithmetique de Presburger. On calcule donc
      Vol(c ; q, b) = card( ... )           (c = variable, q,b = parametres)
      UbCut(q, b)   = ub Vol                (BORNE SUP sur c, pas le max exact)
avec iscc / barvinok, puis on traduit le quasi-polynome par morceaux en C.

Dependances
-----------
  * iscc (suite barvinok) dans le PATH, ou chemin passe via --iscc.

Usage
-----
  # domaine iscc complet en ligne (params et vars extraits automatiquement)
  python3 gen_maxcut.py \\
        --domain "[N,M]->{[i,j,k]: 0<=i<N and 0<=j<=i and 0<=k<M}" \\
        --t 32 --out maxcut.c

  # ou via un fichier contenant le domaine
  python3 gen_maxcut.py --domain domaine.isl --t 32

  --domain accepte : une definition complete '[params]->{[vars]: corps}',
  la forme sans parametres '{[vars]: corps}', ou le corps seul (auquel cas
  fournir --vars). Si la valeur est un fichier existant, son contenu est lu.
  La 1ere variable est l'axe des tranches (x1), la 2eme celui des coupes (x2).
"""

import argparse
import os
import math
import re
import shutil
import subprocess
import sys
import tempfile


# --------------------------------------------------------------------------- #
#  1. Construction des scripts iscc                                           #
# --------------------------------------------------------------------------- #
def build_iscc_min1(constraints, params, varlist):
    """Premier script : recuperer l'expression de min(x1) via lexmin."""
    vars_str = ",".join(varlist)
    x1 = varlist[0]
    par_decl = f"[{','.join(params)}] -> " if params else ""
    s = []
    s.append(f"D := {par_decl}{{ [{vars_str}] : {constraints} }};")
    # projeter sur x1 (garder x1 en sortie), puis lexmin de l'image
    s.append(f"P := {{ [{vars_str}] -> [{x1}] }};")
    s.append("M := lexmin (P(D));")
    s.append("print M;")
    return "\n".join(s)


def build_iscc_max1(constraints, params, varlist):
    """Script symetrique : recuperer l'expression de max(x1) via lexmax,
    pour borner par le haut l'indice de tranche b."""
    vars_str = ",".join(varlist)
    x1 = varlist[0]
    par_decl = f"[{','.join(params)}] -> " if params else ""
    s = []
    s.append(f"D := {par_decl}{{ [{vars_str}] : {constraints} }};")
    s.append(f"P := {{ [{vars_str}] -> [{x1}] }};")
    s.append("M := lexmax (P(D));")
    s.append("print M;")
    return "\n".join(s)


def build_iscc_full(constraints, params, varlist, t, f1_expr,
                    max1_expr=None):
    """
    Script principal : tranche b alignee sur f1_expr, coupe glissante c,
    card puis ub. t est numerique (constante).
    Le quasi-polynome a 'c' pour variable et (params, b) pour parametres.
    Si max1_expr est fourni (= max de x1), on borne b par le haut : la
    tranche b doit commencer dans le domaine, soit f1 + b*t <= max(x1).
    """
    vars_str = ",".join(varlist)
    x1, x2 = varlist[0], varlist[1]
    out_params = list(params) + ["b"]
    par_decl = "[" + ",".join(out_params) + "] -> "

    lo = f"({f1_expr}) + b*{t}"
    hi = f"({f1_expr}) + (b+1)*{t}"
    bmax_clause = ""
    if max1_expr is not None:
        # b >= 0 deja pose ; borne haute : debut de tranche dans le domaine
        bmax_clause = f"and ({f1_expr}) + b*{t} <= ({max1_expr}) "
    s = []
    s.append(f"# t = {t} (constante) ; f1(q) = {f1_expr}"
             + (f" ; max(x1) = {max1_expr}" if max1_expr is not None else ""))
    s.append(
        f"Vol := card {par_decl}{{ [c] -> [{vars_str}] : "
        f"({constraints}) "
        f"and b >= 0 and c >= 0 "
        f"{bmax_clause}"
        f"and {lo} <= {x1} < {hi} "
        f"and c <= {x2} < c + {t} }};"
    )
    s.append("MaxCut := ub Vol;")
    s.append("print MaxCut;")
    return "\n".join(s)


# --------------------------------------------------------------------------- #
#  2. Execution d'iscc                                                         #
# --------------------------------------------------------------------------- #
def run_iscc(script, iscc_bin):
    with tempfile.NamedTemporaryFile("w", suffix=".iscc", delete=False) as f:
        f.write(script)
        path = f.name
    try:
        with open(path) as stdin_f:
            proc = subprocess.run(
                [iscc_bin], stdin=stdin_f,
                capture_output=True, text=True, timeout=120,
            )
    finally:
        os.unlink(path)
    if proc.returncode != 0:
        raise RuntimeError(
            f"iscc a echoue (code {proc.returncode}) :\n{proc.stderr}\n"
            f"--- script ---\n{script}"
        )
    return proc.stdout.strip()


# --------------------------------------------------------------------------- #
#  3. Traduction du quasi-polynome par morceaux en C                          #
# --------------------------------------------------------------------------- #
def isl_floor_to_c(expr):
    """Convertit les floor d'isl en appels FLOORDIV(num, den)."""
    expr = re.sub(
        r"floor\(\s*\((.*?)\)\s*/\s*(\d+)\s*\)",
        lambda m: f"FLOORDIV(({m.group(1)}), {m.group(2)})", expr,
    )
    expr = re.sub(
        r"\[\s*\((.*?)\)\s*/\s*(\d+)\s*\]",
        lambda m: f"FLOORDIV(({m.group(1)}), {m.group(2)})", expr,
    )
    return expr


def insert_implicit_mul(e):
    """isl ecrit '32b' ou '32*b' ; C exige '32*b'. Insere le * manquant
    entre un nombre et une lettre/parenthese, et entre ')' et lettre."""
    # nombre suivi d'une variable ou '(' :  32b -> 32*b , 3(  -> 3*(
    e = re.sub(r"(?<=\d)(?=[A-Za-z_(])", "*", e)
    # ')' suivi d'une variable ou '(' :  )N -> )*N , )( -> )*(
    e = re.sub(r"(?<=\))(?=[A-Za-z_(])", "*", e)
    return e


def parse_max_args(expr):
    """
    Si expr est de la forme  max(a, b, c, ...)  renvoie la liste [a, b, c,...]
    (arguments au niveau 0 de parentheses). Sinon renvoie [expr] tel quel.
    'ub' exprime la borne superieure comme le maximum de plusieurs candidats
    polynomiaux : il faut donc tous les conserver et calculer leur max en C.
    """
    e = expr.strip()
    m = re.match(r"^max\s*\((.*)\)$", e, re.S)
    if not m:
        return [e]
    inner = m.group(1)
    return [a.strip() for a in split_top_level(inner, ",") if a.strip()]


def normalize_term(expr, varnames=None):
    """
    Traduit un morceau de quasi-polynome isl en une LISTE de candidats C.
    'ub' exprime la borne superieure comme max(p1, p2, ...) : on renvoie la
    liste [c(p1), c(p2), ...]. emit_c calcule le maximum effectif.
    Chaque candidat est converti en arithmetique entiere exacte.
    """
    if varnames is None:
        varnames = []
    args = parse_max_args(expr.strip())
    return [rational_poly_to_c(a, varnames) for a in args]


def rational_poly_to_c(expr, varnames):
    """
    Convertit un polynome a coefficients rationnels (et floor) en une
    expression C a arithmetique entiere EXACTE.
    Strategie : mettre sur denominateur commun via sympy, puis emettre
    (numerateur_entier) / denominateur  avec division finale unique.
    Repli regex si sympy absent (gere les cas a coeffs entiers).
    """
    e = expr
    try:
        import sympy
        from sympy import symbols, sympify, together, fraction, Poly, floor
        syms = {v: symbols(v, integer=True) for v in varnames}
        # b^2 -> b**2 ; et inserer le * implicite (5b -> 5*b, 2N -> 2*N)
        e_sym = e.replace("^", "**")
        e_sym = insert_implicit_mul(e_sym)
        # securiser les noms inconnus : on ne fournit que varnames + floor
        loc = dict(syms)
        loc["floor"] = floor
        expr_s = sympify(e_sym, locals=loc, rational=True)
        num, den = fraction(together(expr_s))
        den_int = int(den)
        if den_int == 1:
            return sympy_to_c(num, syms)
        num_c = sympy_to_c(num, syms)
        # division entiere exacte (le resultat est entier par construction)
        return f"FLOORDIV({num_c}, {den_int})"
    except Exception as exc:
        # Le repli ne doit pas produire silencieusement du C faux : signaler,
        # puis passer par la mise au denominateur commun sans sympy.
        sys.stderr.write(
            "gen_maxcut: sympy indisponible ou expression non geree (%s) ;\n"
            "            repli exact sans sympy sur : %s\n" % (exc, e.strip()))
        return normalize_basic(e, varnames)


def sympy_to_c(node, syms):
    """Emet du C a partir d'une expression sympy polynomiale entiere."""
    import sympy
    from sympy import Add, Mul, Pow, Integer, Symbol, floor, Rational
    if isinstance(node, Add):
        return "(" + " + ".join(sympy_to_c(a, syms) for a in node.args) + ")"
    if isinstance(node, Mul):
        return "(" + "*".join(sympy_to_c(a, syms) for a in node.args) + ")"
    if isinstance(node, Pow):
        base = sympy_to_c(node.base, syms)
        k = int(node.exp)
        if k >= 1:
            return "(" + "*".join([base] * k) + ")"
        return f"1 /*pow{k}*/"
    if node.func == floor:
        # floor(p/q) -> FLOORDIV(num, den)
        arg = node.args[0]
        from sympy import fraction, together
        n, d = fraction(together(arg))
        return f"FLOORDIV({sympy_to_c(n, syms)}, {int(d)})"
    if isinstance(node, Integer):
        return str(int(node))
    if isinstance(node, Rational):
        # ne devrait plus arriver apres mise au denominateur commun
        return f"FLOORDIV({int(node.p)}, {int(node.q)})"
    if isinstance(node, Symbol):
        return str(node)
    return str(node)


def _rational_denominators(e):
    """Denominateurs des coefficients rationnels, hors arguments de floor().
    Les divisions internes a floor(...) ont leur propre semantique et ne
    doivent pas etre mises au denominateur commun avec le reste."""
    masked = re.sub(r"floor\s*\([^()]*(?:\([^()]*\)[^()]*)*\)", " FLOORARG ", e)
    return [int(d) for _, d in re.findall(r"(\d+)\s*/\s*(\d+)", masked)]


def _scale_term(term, L):
    """Multiplie un terme par l'entier L en restant exact.
    Un coefficient a/b devient a*(L//b) ; un terme sans coefficient
    rationnel est simplement multiplie par L."""
    masked = re.sub(r"floor\s*\([^()]*(?:\([^()]*\)[^()]*)*\)", " FLOORARG ", term)
    m = re.search(r"(\d+)\s*/\s*(\d+)", masked)
    if not m:
        return "(%d * (%s))" % (L, term.strip()) if term.strip() else term
    a, b = int(m.group(1)), int(m.group(2))
    assert L % b == 0
    # remplacer la premiere occurrence reelle de "a/b" dans le terme
    return re.sub(r"%d\s*/\s*%d" % (a, b), str(a * (L // b)), term, count=1)


def normalize_basic(expr, varnames):
    """Repli sans sympy. Les coefficients rationnels sont mis au denominateur
    commun et la division entiere est faite une seule fois a la fin : emettre
    "49335/4 - 135/2 * b" tel quel tronquerait chaque coefficient
    independamment et donnerait une valeur fausse sans le moindre signe."""
    e = expr.strip()

    dens = _rational_denominators(e)
    if dens:
        L = 1
        for d in dens:
            L = L * d // math.gcd(L, d)
        # decouper en termes au niveau 0, en conservant les signes
        spaced = re.sub(r"(?<=[\w\)\]])\s*([+-])\s*", r" \1 ", e)
        parts, sign, cur = [], "+", []
        for tok in split_top_level(spaced, " "):
            if tok in ("+", "-") and cur:
                parts.append((sign, "".join(cur))); sign, cur = tok, []
            elif tok in ("+", "-") and not cur:
                sign = tok if sign == "+" else "+"
            else:
                cur.append(tok)
        if cur:
            parts.append((sign, "".join(cur)))
        scaled = []
        for sg, term in parts:
            if not term.strip():
                continue
            scaled.append(("- " if sg == "-" else "+ ") + _scale_term(term, L))
        joined = " ".join(scaled).lstrip("+ ").strip()
        e = "FLOORDIV(%s, %d)" % (joined, L)

    def pow_repl(m):
        base, k = m.group(1), int(m.group(2))
        return "(" + "*".join([base] * k) + ")"
    e = re.sub(r"([A-Za-z_]\w*)\^(\d+)", pow_repl, e)
    e = isl_floor_to_c(e)
    e = insert_implicit_mul(e)
    return e


def split_top_level(s, sep):
    """Decoupe sur 'sep' au niveau de parentheses/crochets 0."""
    out, depth, cur = [], 0, []
    for ch in s:
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        if ch == sep and depth == 0:
            out.append("".join(cur)); cur = []
        else:
            cur.append(ch)
    out.append("".join(cur))
    return out


def expand_chained(c):
    """a <= x < b -> ((a)<=(x)) && ((x)<(b))."""
    pat = re.compile(r"([^<>=]+?)\s*(<=|<)\s*([^<>=]+?)\s*(<=|<)\s*([^<>=]+)")
    def repl(m):
        a, op1, x, op2, b = (g.strip() for g in m.groups())
        return f"(({a}) {op1} ({x})) && (({x}) {op2} ({b}))"
    return pat.sub(repl, c)


def isl_cond_to_c(cond):
    """Traduit une condition isl en condition C.
    Decoupe sur 'and'/'or', traduit chaque comparaison atomique, et
    convertit les quantificateurs 'exists (e : ...)' en tests de
    divisibilite (existence d'un multiple dans un intervalle)."""
    if not cond:
        return "1"
    return cond_or(cond.strip())


def exists_to_c(inner):
    """
    Traduit le corps d'un  exists (e : C1 and C2 and ...)  en condition C.
    - Le nom de la variable liee est avant le ':'.
    - Les sous-conditions sans la variable liee sont traduites normalement.
    - Les sous-conditions qui l'encadrent  A <= k*e <= B  deviennent un test
      'il existe un entier e avec A <= k*e <= B', i.e. l'intervalle [A,B]
      contient un multiple de k :
          FLOORDIV(B, k) >= CEILDIV(A, k)
      avec CEILDIV(A,k) = FLOORDIV(A + k - 1, k) pour k > 0.
    On traite le cas usuel d'un seul coefficient k pour e.
    """
    if ":" in inner:
        var, body = inner.split(":", 1)
        var = var.strip()
    else:
        var, body = "e0", inner
    body = insert_implicit_mul(body.strip())  # 2e0 -> 2*e0 (frontiere de mot)

    conj = split_logical(body, "and")
    free_terms = []
    lowers = []
    uppers = []
    coeff = None

    for part in conj:
        part = part.strip()
        if re.search(r"\b" + re.escape(var) + r"\b", part):
            k, lo, hi = parse_e_bound(part, var)
            if k is None:
                free_terms.append("/*unrecognized_exists_term*/ 1")
                continue
            coeff = k if coeff is None else coeff
            if lo is not None:
                lowers.append(lo)
            if hi is not None:
                uppers.append(hi)
        else:
            free_terms.append(atom_to_c(part))

    tests = list(free_terms)
    if coeff is not None and lowers and uppers:
        A = lowers[0] if len(lowers) == 1 else "MAXEXPR(" + \
            ", ".join(lowers) + ")"
        B = uppers[0] if len(uppers) == 1 else "MINEXPR(" + \
            ", ".join(uppers) + ")"
        tests.append(
            f"(FLOORDIV(({B}), {coeff}) >= "
            f"FLOORDIV(({A}) + {coeff} - 1, {coeff}))")
    elif coeff is not None:
        tests.append("1")

    if not tests:
        return "1"
    return " && ".join("(" + t + ")" for t in tests)


def parse_e_bound(part, var):
    """
    Analyse une sous-condition portant sur la variable liee 'var'.
    Reconnait A <= k*var <= B (chainee), k*var <= B, A <= k*var,
    k*var >= A, B >= k*var. Renvoie (k, A_or_None, B_or_None) en C, ou
    (None, None, None) si non reconnu. k = coefficient de var (defaut 1).
    """
    p = insert_implicit_mul(part.strip())  # 2e0 -> 2*e0
    mco = re.search(r"(\d+)\s*\*\s*" + re.escape(var) + r"\b", p)
    k = int(mco.group(1)) if mco else 1
    if mco:
        termre = re.escape(f"{k}*{var}")
    else:
        termre = r"\b" + re.escape(var) + r"\b"

    m = re.match(r"^(.+?)\s*(?:<=|<)\s*" + termre + r"\s*(?:<=|<)\s*(.+)$", p)
    if m:
        return k, fix_atom(m.group(1)), fix_atom(m.group(2))
    m = re.match(r"^" + termre + r"\s*(?:<=|<)\s*(.+)$", p)
    if m:
        return k, None, fix_atom(m.group(1))
    m = re.match(r"^(.+?)\s*(?:<=|<)\s*" + termre + r"$", p)
    if m:
        return k, fix_atom(m.group(1)), None
    m = re.match(r"^" + termre + r"\s*(?:>=|>)\s*(.+)$", p)
    if m:
        return k, fix_atom(m.group(1)), None
    m = re.match(r"^(.+?)\s*(?:>=|>)\s*" + termre + r"$", p)
    if m:
        return k, None, fix_atom(m.group(1))
    return None, None, None


def cond_or(c):
    c = c.strip()
    # dépouiller une paire de parentheses externes englobant tout l'atome
    c = strip_outer_parens(c)
    # un 'exists(...)' englobant tout l'atome
    me = re.match(r"^exists\s*\((.*)\)$", c, re.S)
    if me:
        return exists_to_c(me.group(1))
    parts = split_logical(c, "or")
    if len(parts) > 1:
        return " || ".join("(" + cond_or(p) + ")" for p in parts)
    parts = split_logical(c, "and")
    if len(parts) > 1:
        return " && ".join("(" + cond_or(p) + ")" for p in parts)
    return atom_to_c(c.strip())


def strip_outer_parens(c):
    """Retire une (ou plusieurs) paire(s) de parentheses englobant tout c."""
    c = c.strip()
    while c.startswith("(") and c.endswith(")"):
        depth = 0
        ok = True
        for i, ch in enumerate(c):
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0 and i != len(c) - 1:
                    ok = False
                    break
        if ok:
            c = c[1:-1].strip()
        else:
            break
    return c


def split_logical(c, kw):
    """Decoupe sur ' kw ' au niveau 0 de parentheses/crochets."""
    out, depth, cur, i = [], 0, [], 0
    n = len(c)
    pad = " " + kw + " "
    while i < n:
        ch = c[i]
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        if depth == 0 and c[i:i+len(pad)] == pad:
            out.append("".join(cur))
            cur = []
            i += len(pad)
            continue
        cur.append(ch)
        i += 1
    out.append("".join(cur))
    return [p.strip() for p in out if p.strip()]


def atom_to_c(a):
    """Traduit une comparaison atomique isl en C.
    Gere les chaines a<=x<b -> (a<=x)&&(x<b) et l'egalite '=' -> '=='.
    Si un membre contient encore un connecteur 'and'/'or' (atome mal
    delimite), on renvoie a cond_or pour redecouper."""
    a = strip_outer_parens(a.strip())
    if not a:
        return "1"
    # si l'atome contient encore des connecteurs logiques de niveau 0,
    # ce n'etait pas un atome : redecouper.
    if len(split_logical(a, "or")) > 1 or len(split_logical(a, "and")) > 1:
        return cond_or(a)
    # comparaison chainee a OP x OP b
    m = re.match(
        r"^([^<>=]+?)\s*(<=|<|>=|>)\s*([^<>=]+?)\s*(<=|<|>=|>)\s*([^<>=]+)$",
        a)
    if m:
        p, op1, x, op2, q = (g.strip() for g in m.groups())
        return (f"(({fix_atom(p)}) {op1} ({fix_atom(x)})) && "
                f"(({fix_atom(x)}) {op2} ({fix_atom(q)}))")
    # comparaison simple
    m = re.match(r"^(.+?)\s*(<=|>=|!=|<|>|=)\s*(.+)$", a)
    if m:
        lhs, op, rhs = m.group(1), m.group(2), m.group(3)
        op = "==" if op == "=" else op
        return f"({fix_atom(lhs)}) {op} ({fix_atom(rhs)})"
    return fix_atom(a)


def fix_atom(s):
    """Nettoie un membre de comparaison : floor, mod, * implicite."""
    s = s.strip()
    s = isl_floor_to_c(s)
    # 'EXPR mod k' -> '(EXPR) % k'  (isl ecrit le modulo avec le mot 'mod')
    s = re.sub(r"\bmod\b", "%", s)
    s = insert_implicit_mul(s)
    return s


def parse_pw_qpoly(isl_out, params, varnames):
    """
    Parse une sortie pw-qpolynomial isl.
    Tolere un prefixe '[params] -> ' et prend le DERNIER bloc { ... }
    (ub peut afficher des infos avant). Renvoie [(cond_C, expr_C), ...].
    varnames : noms apparaissant dans le polynome (params du domaine + 'b').
    """
    if not isl_out or not isl_out.strip():
        raise ValueError(
            "Sortie iscc VIDE. Verifier que iscc imprime bien le resultat "
            "(commande 'print') et que le script ne plante pas. Relancer "
            "les scripts iscc a la main pour diagnostiquer.")
    # prendre le dernier groupe { ... } equilibre de la sortie
    body = extract_last_braces(isl_out)
    if body is None:
        # peut etre une constante nue
        return [("1", normalize_term(isl_out.strip(), varnames))]
    pieces = []
    for chunk in split_top_level(body, ";"):
        chunk = chunk.strip()
        if not chunk:
            continue
        if ":" in chunk:
            expr, cond = chunk.split(":", 1)
        else:
            expr, cond = chunk, ""
        expr = expr.strip()
        if not expr:
            continue
        pieces.append((isl_cond_to_c(cond.strip()),
                       normalize_term(expr, varnames)))
    if not pieces:
        raise ValueError(f"Aucun morceau parse dans : {isl_out!r}")
    return pieces


def extract_last_braces(s):
    """Renvoie le contenu du dernier groupe {...} equilibre, ou None."""
    # trouver toutes les positions d'ouverture
    last = None
    i = 0
    n = len(s)
    while i < n:
        if s[i] == "{":
            depth = 1
            j = i + 1
            while j < n and depth > 0:
                if s[j] == "{":
                    depth += 1
                elif s[j] == "}":
                    depth -= 1
                j += 1
            if depth == 0:
                last = s[i + 1 : j - 1].strip()
            i = j
        else:
            i += 1
    return last


# --------------------------------------------------------------------------- #
#  4. Emission du fichier C                                                    #
# --------------------------------------------------------------------------- #
C_HEADER = """\
/* Automatically generated by gen_maxcut.py
 *
 * {func}(params..., b) :
 *   UPPER BOUND on the volume of the x2 cross-sections (height t = {t}) of
 *   slice b. Slices are aligned on min(x1), with width t = {t}.
 *
 *   NOTE: computed with barvinok's 'ub' operator. This is a guaranteed
 *   UPPER BOUND (always >= the true maximum, never below it), not the exact
 *   integer maximum: it may exceed the true value slightly.
 *
 * DO NOT EDIT BY HAND: regenerate from the source domain.
 */
#include <stdint.h>

/* floor division (round toward minus infinity), correct for negative
 * numerators. Defined as a MACRO (not 'static inline') so it stays valid
 * even when this code is included inside another file or function. */
#ifndef FLOORDIV
#define FLOORDIV(num, den) \\
    ( ((num) % (den) != 0 && (((num) % (den) < 0) != ((den) < 0))) \\
        ? ((num) / (den) - 1) : ((num) / (den)) )
#endif
#ifndef MAXEXPR
#define MAXEXPR(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MINEXPR
#define MINEXPR(a, b) ((a) < (b) ? (a) : (b))
#endif

"""


def emit_c(pieces, params, func_name, t):
    sig_params = ", ".join(f"long {p}" for p in params)
    sig = (f"long {func_name}({sig_params}, long b)"
           if sig_params else f"long {func_name}(long b)")
    out = [C_HEADER.format(func=func_name, t=t)]
    out.append(sig + " {")
    for cond, cands in pieces:
        # cands : candidats de la borne sup ; 'ub' les combine par max(...)
        out.append(f"    if ({cond}) {{")
        if len(cands) == 1:
            out.append(f"        return (long)({cands[0]});")
        else:
            # calcul du maximum des candidats via un tableau local
            n = len(cands)
            out.append(f"        long __c[{n}];")
            for i, expr in enumerate(cands):
                out.append(f"        __c[{i}] = (long)({expr});")
            out.append("        long __m = __c[0];")
            out.append(f"        for (int __i = 1; __i < {n}; __i++)")
            out.append("            if (__c[__i] > __m) __m = __c[__i];")
            out.append("        return __m;")
        out.append("    }")
    out.append("    return 0; /* outside the domain: no cross-section */")
    out.append("}")
    out.append("")
    return "\n".join(out)


# --------------------------------------------------------------------------- #
#  5. Programme principal                                                      #
# --------------------------------------------------------------------------- #
def read_domain(args):
    """Lit le domaine depuis --domain (chaine inline OU chemin de fichier)
    ou --constraints (alias). Renvoie le texte brut, sans interpretation."""
    src = args.domain if args.domain is not None else args.constraints
    if src is None:
        print("Erreur : fournir le domaine via --domain (ou --constraints).",
              file=sys.stderr)
        sys.exit(2)
    # si la valeur designe un fichier existant, lire son contenu ;
    # sinon la traiter comme le domaine inline.
    if os.path.exists(src) and os.path.isfile(src):
        with open(src) as f:
            return f.read().strip()
    return src.strip()


def parse_domain(raw):
    """
    Parse une entree qui peut etre :
      (a) une definition iscc complete :  [p1,p2] -> { [v1,v2,v3] : CORPS }
      (b) idem sans parametres          :  { [v1,v2] : CORPS }
      (c) le corps seul des contraintes :  CORPS

    Renvoie (params, vars, corps) :
      - params : liste (eventuellement vide) des parametres declares
      - vars   : liste (eventuellement vide) des variables declarees
      - corps  : la chaine des contraintes (ce qui suit ':')
    Quand la forme est (c), params et vars sont vides : l'appelant devra
    se rabattre sur --params/--vars.
    """
    s = raw.strip()
    params, varnames, body = [], [], None

    # --- prefixe parametres : [ ... ] ->
    m = re.match(r"^\s*\[([^\]]*)\]\s*->\s*(.*)$", s, re.S)
    if m:
        params = [p.strip() for p in m.group(1).split(",") if p.strip()]
        s = m.group(2).strip()

    # --- bloc { [vars] : corps }
    if s.startswith("{"):
        inner = s[1:].strip()
        # retirer une eventuelle accolade fermante finale (et parasites)
        inner = inner.rstrip()
        while inner.endswith("}"):
            inner = inner[:-1].rstrip()
        # forme '[v1,v2,...] : corps'
        mv = re.match(r"^\s*\[([^\]]*)\]\s*:\s*(.*)$", inner, re.S)
        if mv:
            varnames = [v.strip() for v in mv.group(1).split(",")
                        if v.strip()]
            body = mv.group(2).strip()
        else:
            # pas de tuple de variables explicite : corps seul dans les {}
            if ":" in inner:
                body = inner.split(":", 1)[1].strip()
            else:
                body = inner.strip()
    else:
        # pas d'accolade : peut etre 'CORPS' nu, ou '[vars] : corps' nu
        mv = re.match(r"^\s*\[([^\]]*)\]\s*:\s*(.*)$", s, re.S)
        if mv:
            varnames = [v.strip() for v in mv.group(1).split(",")
                        if v.strip()]
            body = mv.group(2).strip()
        elif ":" in s and "->" not in s:
            body = s.split(":", 1)[1].strip()
        else:
            body = s.strip()

    # nettoyage accolades parasites residuelles
    if body is not None:
        body = body.strip()
        while body.endswith("}"):
            body = body[:-1].rstrip()
        while body.startswith("{"):
            body = body[1:].lstrip()
    return params, varnames, body


def extract_f1(min1_out, params):
    """Extrait l'expression affine de min(x1) depuis la sortie lexmin.
    Formats possibles : '{ [0] }', '[N] -> { [(N)] }', '{ [x] : x = 0 }'."""
    if not min1_out or not min1_out.strip():
        return "0"  # defaut : tranches alignees sur 0
    body = extract_last_braces(min1_out)
    if body is None:
        return "0"
    # forme '[x] : x = EXPR'
    m = re.search(r"=\s*([^:}\]]+)", body)
    if m:
        return normalize_term(m.group(1).strip(), params)[0]
    # forme '[ EXPR ]'
    m = re.search(r"\[\s*([^\]]+?)\s*\]", body)
    if m:
        val = m.group(1).strip()
        # si c'est juste le nom de variable (min non contraint), defaut 0
        return normalize_term(val, params)[0]
    return "0"


INJECT_BEGIN = "/* === BEGIN gen_maxcut (genere automatiquement) === */"
INJECT_END = "/* === END gen_maxcut === */"


def inject_into_c(target_path, c_code, dest_path=None, backup=False):
    """
    Lit le fichier C source 'target_path', insere c_code juste avant la
    definition de main, et ecrit le resultat.
    - dest_path : si fourni, le resultat est ecrit la (l'original n'est pas
      modifie). Sinon, l'ecriture se fait sur target_path (en place).
    - Si une insertion precedente existe (entre les marqueurs) dans la
      source, elle est remplacee (pas de doublon).
    - backup : si True ET ecriture en place, cree un .bak de l'original.
    """
    with open(target_path, "r") as f:
        src = f.read()

    block = INJECT_BEGIN + "\n" + c_code.rstrip() + "\n" + INJECT_END + "\n\n"

    pat_prev = re.compile(
        re.escape(INJECT_BEGIN) + r".*?" + re.escape(INJECT_END) + r"\n*",
        re.S,
    )
    if pat_prev.search(src):
        new_src = pat_prev.sub(block, src, count=1)
        status = "remplace (insertion precedente mise a jour)"
    else:
        pos = find_main_definition(src)
        if pos is None:
            raise RuntimeError(
                f"Impossible de localiser la definition de 'main' dans "
                f"{target_path}. Verifier que le fichier contient un main.")
        new_src = src[:pos] + block + src[pos:]
        status = "insere avant main"

    out_path = dest_path if dest_path else target_path
    in_place = (out_path == target_path)
    with open(out_path, "w") as f:
        f.write(new_src)
    if backup and in_place:
        with open(target_path + ".bak", "w") as f:
            f.write(src)
    return status, out_path


def find_main_definition(src):
    """
    Renvoie l'indice du debut de la definition de main (debut de sa ligne),
    en ignorant commentaires et chaines. Heuristique robuste :
      cherche un motif  <type> main ( ... )  suivi (apres espaces) de '{'.
    """
    masked = mask_comments_strings(src)
    # type de retour optionnel + main + ( ... ) + { eventuellement sur
    # lignes suivantes
    for m in re.finditer(r"\bmain\b\s*\(", masked):
        # verifier qu'apres la parenthese fermante vient un '{'
        i = m.end() - 1  # position du '('
        depth = 0
        j = i
        n = len(masked)
        while j < n:
            if masked[j] == "(":
                depth += 1
            elif masked[j] == ")":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        # apres j (la ')'), chercher le premier non-espace
        k = j + 1
        while k < n and masked[k] in " \t\r\n":
            k += 1
        if k < n and masked[k] == "{":
            # c'est bien une DEFINITION de main (pas un prototype/appel)
            # reculer au debut de la ligne contenant le type de retour
            start = src.rfind("\n", 0, m.start()) + 1
            return start
    return None


def mask_comments_strings(src):
    """Remplace commentaires et chaines par des espaces (preserve indices)."""
    out = list(src)
    i, n = 0, len(src)
    state = None  # None, 'line', 'block', 'str', 'char'
    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ""
        if state is None:
            if c == "/" and nxt == "/":
                state = "line"; out[i] = out[i + 1] = " "; i += 2; continue
            if c == "/" and nxt == "*":
                state = "block"; out[i] = out[i + 1] = " "; i += 2; continue
            if c == '"':
                state = "str"; i += 1; continue
            if c == "'":
                state = "char"; i += 1; continue
            i += 1
        elif state == "line":
            if c == "\n":
                state = None
            else:
                out[i] = " "
            i += 1
        elif state == "block":
            if c == "*" and nxt == "/":
                out[i] = out[i + 1] = " "; i += 2; state = None; continue
            if c != "\n":
                out[i] = " "
            i += 1
        elif state == "str":
            if c == "\\":
                i += 2; continue
            if c == '"':
                state = None
            i += 1
        elif state == "char":
            if c == "\\":
                i += 2; continue
            if c == "'":
                state = None
            i += 1
    return "".join(out)


def assemble_full_script(s_min1, s_full):
    """Concatene les deux passes iscc en un seul script lisible."""
    return (
        "# ============================================================\n"
        "#  Script iscc complet genere par gen_maxcut.py\n"
        "#  Passe 1 : min(x1) pour aligner les tranches\n"
        "#  Passe 2 : card du volume de coupe (c variable, b parametre)\n"
        "#            puis ub -> borne sup du volume de coupe par tranche\n"
        "# ============================================================\n\n"
        "# ---------- Passe 1 : min(x1) ----------\n"
        + s_min1 + "\n\n"
        "# ---------- Passe 2 : MaxCut ----------\n"
        + s_full + "\n"
    )


def emit_dump(text, dest):
    """Ecrit le dump sur stdout (dest == '-') ou dans un fichier."""
    if dest == "-":
        print(text)
    else:
        with open(dest, "w") as f:
            f.write(text)
        print(f"Script iscc complet ecrit dans {dest}")


ISCC_KEYWORDS = {"max", "min", "ceild", "floord", "exists", "and", "or",
                 "floor", "ceil", "true", "false"}


def warn_undeclared_identifiers(constraints, params, varlist):
    """
    Avertit si des identifiants apparaissent dans les contraintes sans etre
    declares comme variable (--vars) ni parametre (--params). Cas typique :
    un parametre Polybench '_PB_N' oublie dans --params.
    """
    idents = set(re.findall(r"[A-Za-z_]\w*", constraints))
    known = set(params) | set(varlist) | ISCC_KEYWORDS
    unknown = sorted(i for i in idents if i not in known)
    if unknown:
        print("Attention : identifiants non declares dans les contraintes : "
              + ", ".join(unknown) + ".\n"
              "  -> S'il s'agit de parametres du domaine, ajoutez-les avec "
              "--params (ex: --params " + ",".join(unknown) + ").\n"
              "  Sinon iscc produira un ensemble vide (sorties VIDES).",
              file=sys.stderr)


def is_safe_f1(expr):
    """
    Verifie que l'expression f1 (min x1) est une expression arithmetique
    simple : pas de mots-cles logiques, parentheses equilibrees, non vide.
    Une sortie lexmin piecewise produit du 'and'/'or'/'=' -> non s2r.
    """
    if not expr or not expr.strip():
        return False
    bad = (" and ", " or ", ">=", "<=", "=", "<", ">", ":", ";")
    e = " " + expr + " "
    if any(tok in e for tok in bad):
        return False
    if expr.count("(") != expr.count(")"):
        return False
    return True


def main():
    ap = argparse.ArgumentParser(
        description="Genere une fonction C : borne sup du volume des coupes "
                    "par tranche (via iscc/barvinok 'ub').")
    ap.add_argument("--domain", help="domaine isl, en ligne ou via un "
                    "fichier. Accepte une definition complete "
                    "'[params]->{[vars]: corps}', la forme sans parametres "
                    "'{[vars]: corps}', ou le corps seul. Si la valeur est "
                    "un chemin de fichier existant, son contenu est lu.")
    ap.add_argument("--constraints", help="alias de --domain (compatibilite)")
    ap.add_argument("--t", type=int, required=True,
                    help="largeur/hauteur des tranches/coupes (constante)")
    ap.add_argument("--params", default=None,
                    help="parametres du domaine (ex: N,M). Optionnel si le "
                         "domaine declare deja '[params] ->'. Surcharge "
                         "ce qui est extrait du domaine.")
    ap.add_argument("--vars", default=None,
                    help="variables du domaine (ex: i,j,k). Optionnel si le "
                         "domaine declare deja '{ [vars] : ... }'. "
                         "x1=1ere, x2=2eme. Surcharge l'extraction.")
    ap.add_argument("--func", default="max_cut_volume",
                    help="nom de la fonction C generee")
    ap.add_argument("--f1", metavar="EXPR",
                    help="force l'expression de min(x1) au lieu de la calculer "
                         "par lexmin. Utile quand la borne inf de x1 est "
                         "evidente (ex: --f1 1) ou quand lexmin renvoie une "
                         "forme piecewise compliquee.")
    ap.add_argument("--allow-exists", action="store_true",
                    help="ecrire le fichier meme si un terme exists n'a pas ete traduit")
    ap.add_argument("--no-bmax", action="store_true",
                    help="ne pas borner l'indice de tranche b par le haut "
                         "(par defaut b est borne via max(x1) pour eliminer "
                         "les tranches hors domaine).")
    ap.add_argument("--bmax", metavar="EXPR",
                    help="force l'expression de max(x1) (borne haute de b) "
                         "au lieu de la calculer par lexmax.")
    ap.add_argument("--iscc", default="iscc", help="chemin de l'executable iscc")
    ap.add_argument("--out", default="maxcut.c", help="fichier C de sortie")
    ap.add_argument("--inject", metavar="PROG.c",
                    help="insere la fonction generee juste avant le main() "
                         "et ecrit le resultat dans PROG_t<T>.c "
                         "(l'original PROG.c reste inchange)")
    ap.add_argument("--no-backup", action="store_true",
                    help="sans effet avec --inject (l'original n'est jamais "
                         "modifie) ; reserve a un usage en place futur")
    ap.add_argument("--print-isl", action="store_true",
                    help="affiche la sortie iscc brute (debug)")
    ap.add_argument("--dump-iscc", nargs="?", const="-", metavar="FICHIER",
                    help="affiche le script iscc complet (2 passes) sur la "
                         "sortie standard, ou l'ecrit dans FICHIER si donne. "
                         "Le script utilise la vraie valeur de min(x1) si "
                         "iscc est disponible, sinon un marqueur F1_X1.")
    args = ap.parse_args()

    raw = read_domain(args)
    dom_params, dom_vars, constraints = parse_domain(raw)
    if constraints is None or not constraints.strip():
        print("Erreur : impossible d'extraire les contraintes du domaine.",
              file=sys.stderr)
        sys.exit(2)

    # priorite : option CLI explicite > extraction du domaine
    if args.params is not None:
        params = [p for p in args.params.split(",") if p]
    else:
        params = dom_params
    if args.vars is not None:
        varlist = [v for v in args.vars.split(",") if v]
    else:
        varlist = dom_vars

    if len(varlist) < 2:
        print("Erreur : au moins 2 variables requises (x1, x2).\n"
              "  Les variables n'ont pas pu etre extraites du domaine "
              "(pas de '{ [v1,v2,...] : ... }') ; fournissez --vars.",
              file=sys.stderr)
        sys.exit(2)

    if args.print_isl:
        print(f"=== domaine parse ===\n  params : {params}\n"
              f"  vars   : {varlist}\n  corps  : {constraints}\n")

    warn_undeclared_identifiers(constraints, params, varlist)

    have_iscc = (shutil.which(args.iscc) is not None
                 or os.path.exists(args.iscc))

    s_min1 = build_iscc_min1(constraints, params, varlist)
    # f1 force par l'utilisateur ? alors on n'execute pas la passe 1.
    f1_forced = args.f1.strip() if args.f1 else None
    preview_f1 = f1_forced if f1_forced else "F1_X1"
    s_full_preview = build_iscc_full(constraints, params, varlist,
                                     args.t, preview_f1)

    if not have_iscc:
        if args.dump_iscc:
            emit_dump(assemble_full_script(s_min1, s_full_preview),
                      args.dump_iscc)
        print("iscc introuvable. Scripts iscc a executer manuellement :\n",
              file=sys.stderr)
        print("=== passe 1 : min(x1) ===")
        print(s_min1)
        print("\n=== passe 2 : MaxCut (remplacer F1_X1 par min(x1)) ===")
        print(s_full_preview)
        sys.exit(1)

    if f1_forced:
        f1_expr = f1_forced
        if args.print_isl:
            print(f"=== min(x1) force par --f1 : {f1_expr} ===\n")
    else:
        min1_out = run_iscc(s_min1, args.iscc)
        if args.print_isl:
            print("=== sortie min(x1) ===\n" + repr(min1_out) + "\n")
        if not min1_out.strip():
            print("ATTENTION : sortie min(x1) VIDE. Script envoye a iscc :\n",
                  file=sys.stderr)
            print(s_min1, file=sys.stderr)
            print("\nLancez-le a la main : iscc < script  pour diagnostiquer.",
                  file=sys.stderr)
        f1_expr = extract_f1(min1_out, params)
        if not is_safe_f1(f1_expr):
            print("ATTENTION : min(x1) extrait semble invalide ou piecewise :"
                  f"\n  f1 = {f1_expr!r}\n"
                  "  La sortie lexmin est probablement par morceaux. "
                  "Fournissez la borne inf de x1 a la main avec --f1 "
                  "(ex: --f1 1 si la 1ere variable verifie 1<=x).",
                  file=sys.stderr)
            sys.exit(1)

    # borne haute de b : max(x1), sauf si desactive ou f1 force sans iscc
    max1_expr = None
    if not args.no_bmax:
        if args.bmax is not None:
            max1_expr = args.bmax.strip()
        else:
            s_max1 = build_iscc_max1(constraints, params, varlist)
            max1_out = run_iscc(s_max1, args.iscc)
            if args.print_isl:
                print("=== sortie max(x1) ===\n" + repr(max1_out) + "\n")
            cand = extract_f1(max1_out, params)  # meme extracteur que min
            if is_safe_f1(cand):
                max1_expr = cand
            else:
                print("Info : max(x1) non exploitable (piecewise) ; b ne "
                      "sera pas borne par le haut. Utilisez --bmax pour le "
                      "forcer.", file=sys.stderr)

    s_full = build_iscc_full(constraints, params, varlist, args.t, f1_expr,
                             max1_expr=max1_expr)

    if args.dump_iscc:
        emit_dump(assemble_full_script(s_min1, s_full), args.dump_iscc)

    full_out = run_iscc(s_full, args.iscc)
    if args.print_isl:
        print("=== sortie MaxCut ===\n" + repr(full_out) + "\n")
    if not full_out.strip():
        print("ERREUR : sortie MaxCut VIDE. iscc n'a rien renvoye.\n"
              "Script envoye a iscc :\n", file=sys.stderr)
        print(s_full, file=sys.stderr)
        print("\nLancez ce script a la main (iscc < fichier) et collez la "
              "sortie pour diagnostic.", file=sys.stderr)
        sys.exit(1)

    if "exists" in full_out:
        print("Attention : 'exists' present dans la sortie ; verifier le C.",
              file=sys.stderr)

    pieces = parse_pw_qpoly(full_out, params, params + ["b"])
    c_code = emit_c(pieces, params, args.func, args.t)

    # Un terme existentiel non traduit devient "1" dans la condition, donc la
    # branche s'applique toujours et peut renvoyer le polynome d'un autre cas.
    # Mieux vaut refuser que d'ecrire un fichier faux sans le dire.
    if "unrecognized_exists_term" in c_code and not args.allow_exists:
        print("gen_maxcut: un terme 'exists' n'a pas pu etre traduit ; la\n"
              "            condition serait toujours vraie et le resultat faux.\n"
              "            Sur un domaine de periode 2, essayez un --t pair.\n"
              "            Utilisez --allow-exists pour ecrire quand meme.",
              file=sys.stderr)
        sys.exit(1)

    with open(args.out, "w") as f:
        f.write(c_code)
    print(f"Fonction C ecrite dans {args.out}")

    if args.inject:
        try:
            base, ext = os.path.splitext(args.inject)
            dest = f"{base}_t{args.t}{ext}"
            status, out_path = inject_into_c(
                args.inject, c_code, dest_path=dest,
                backup=not args.no_backup)
            print(f"Injection : {status}")
            print(f"Programme transforme ecrit dans {out_path} "
                  f"(original {args.inject} inchange)")
        except Exception as e:
            print(f"Echec injection : {e}", file=sys.stderr)
            sys.exit(1)
    else:
        print("\n--- apercu ---")
        print(c_code)


if __name__ == "__main__":
    main()
