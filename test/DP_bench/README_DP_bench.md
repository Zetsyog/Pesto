# DP_bench — non-serial polyadic dynamic-programming kernels

A collection of dynamic-programming kernels sharing a single structure: an entry
of a table is computed from entries at *shorter intervals* or *lower indices*,
through an inner reduction whose length grows with the distance to the diagonal.
They are the kernels that classical rectangular tiling handles poorly, because
the amount of parallel work varies sharply across the iteration domain.

Each kernel lives in its own directory and is a self-contained C program with a
`#pragma scop` / `#pragma endscop` region, so it can be fed directly to a
source-to-source polyhedral tool.

## Building and running

The two shared headers are in `include/`:

- `mem.h` — allocation helpers (`mem`, `memd`, `mem3d`, `rna_seq_alloc`),
  the `MIN`/`MAX` macros, the random sequence generator, and the array printers;
- `metrics.h` — timing, enabled by `-DMETRICS_TIME`, and the `RSEED` default.

    gcc -O3 -march=native -fopenmp -I ../include kernel.c -o kernel \
        -DN=2000 -DRSEED=1 -DMETRICS_TIME -lm

| flag | effect |
|---|---|
| `-DN=<size>` | problem size; `DIM` is `N+10`, leaving room for the borders |
| `-DRSEED=<n>` | seed of the random input, fixed for reproducible runs |
| `-DMETRICS_TIME` | print the kernel time on stdout |
| `-DRESULT_DUMP` | print the result table on stderr, for comparing two versions |

`RESULT_DUMP` is what makes a transformed version checkable: run the original and
the transformed program with the same `N` and `RSEED`, and diff the two dumps.

## The kernels

### Interval dynamic programming

The table is indexed by an interval $[i,j]$ and the recurrence splits it at every
interior point $k$. Skewing the outer index to the interval length $j-i$ makes the
fronts parallel, since every read then falls at a strictly shorter interval.

**`knuth`** — optimal binary search tree, in the form Knuth gave it.
`c[i][j] = min_k (w[i][j] + c[i][k] + c[k][j])`. One statement, one reduction,
no initialization to hoist: the smallest kernel of the family, and the reference
against which the others' complications are measured.

**`triang`** — minimum-weight triangulation of a convex polygon. Same shape as
`knuth`, with the cost of a triangle in place of the weight, and written with the
outer loop already on the gap. The initialization is folded into the reduction by
a ternary on the first value of `k`, which is worth knowing: there is no separate
initialization statement for a tool to hoist.

**`mcm`** — matrix-chain multiplication. The reduction minimizes the cost of the
two sub-chains plus the product of the three dimensions. The outer loop is on the
chain length, so the kernel comes pre-skewed; the initialization to `INT_MAX` is
a separate statement, ahead of the reduction.

**`nussinov`** — RNA secondary structure prediction, maximizing the number of
base pairs. Two reduction nests per point: the split over `k`, and a
single-iteration nest carrying the diagonal term `S[i+1][j-1] + can_pair(i,j)`.
The second is written as a loop of trip count one, which keeps the nest
rectangular for tools that dislike guards.

**`counting`** — counts the secondary structures of an RNA sequence. Same
splitting as `nussinov` with a sum in place of the maximum, so the result is a
count rather than a score, and the arithmetic saturates far sooner.

**`counting_im`** — the same count with the *interior/exterior* decomposition:
the reduction runs to `j-1` and a finalization `c[i][j] += c[i][j-1]` follows it
outside the `k` loop. That trailing statement is what distinguishes it: a tiling
of the reduction has to place it after the tile loop, not inside.

**`mcc`** — McCaskill partition function, the equilibrium counterpart of
`nussinov`. Two arrays: `Q` accumulates the partition function over an interval,
`Qbp` the contribution of a closing pair. Note that `Qbp[k+i][j]` depends only on
`k+i` and `j`, so the same cell is written by every `(i,k)` of equal sum — the
kernel recomputes it many times over, and a scheduler that makes `i+k` a loop
index exposes the redundancy to loop-invariant code motion while one that does
not, does not.

**`zuker`** — RNA folding with a full energy model. The most irregular kernel of
the set: four levels of loops, an interior reduction over `(k,m)` with a
distance-window guard, two statements at the `k` level and two finalizations at
the `(i,j)` level. Everything the simpler kernels raise separately appears here at
once.

**`mea`** — maximum expected accuracy folding, in five successive nests: the
McCaskill forward pass, the base-pair probabilities, the unpaired probabilities,
their marginals, and a final traceback-scoring pass. The comments in the source
mark which nests are trapezoidal and which have a directly parallel loop. The
longest program of the set, and the only one chaining several kernels of
different shapes.

### Sequence alignment

The table is indexed by a pair of positions and the recurrence looks back along
each axis. Skewing the outer index to the anti-diagonal $i+j$ makes the fronts
parallel.

**`nw`** — Needleman–Wunsch global alignment with an affine-free gap penalty:
the gap cost `W[k]` is paid explicitly for every length, so each cell carries two
reductions, one scanning the column and one the row, plus the diagonal
substitution term. The borders carry the gap penalty and negative scores are
kept, which is what makes the alignment global.

**`sw`** — Smith–Waterman local alignment. The same recurrence with two changes
that define local alignment: the borders are zero, and the score is clamped at
zero so that a poor prefix can be abandoned and a new alignment started.

**`sw3d`** — Smith–Waterman on three sequences. Six reduction nests per point:
three along the axes and three along the diagonal planes, plus the fully
diagonal term. The parallel front is two-dimensional and its area grows
quadratically, so the imbalance across fronts is the sharpest of the set.

## What these kernels exercise

They are ordered above roughly by how much they ask of a tiling tool:

- **one reduction nest, nothing around it** — `knuth`, `triang`;
- **an initialization to hoist above the reduction** — `mcm`, `nussinov`;
- **a finalization to sink below it** — `counting_im`;
- **several reduction nests per point, with different bounds** — `nw`, `sw`,
  `zuker`, `sw3d`;
- **several kernels of different shapes in one scop** — `mea`.

The second dimension of difficulty is the shape of the reduction bound. In the
interval kernels its length depends on the gap alone, so the work is constant
along a front. In `nw` and `sw` the two nests scan $x-y$ and $y$ values
respectively — neither is independent of the position along the front, only their
sum is. Any tool that assumes the first shape will silently unbalance the second.
