# Measuring load balance

Two files are enough: `loadbalance.h`, which carries the machinery, and
`instrument_omp.awk`, which injects it into a generated code. The script knows
nothing about the kernel and works equally on PESTO and Pluto output, which is
what makes the two measurements comparable — the same code produces them.

## What is measured

Two indicators, summed **region by region** and then reported as a ratio. An
imbalance that reverses from one region to the next would cancel out in a global
total although it costs at every barrier; summing per region avoids that
artefact.

- **volume** — number of iteration points executed by each thread. Exact,
  reproducible, and directly comparable between two versions since the domain is
  the same. This is the indicator to rely on for an argument.
- **time** — wall time each thread spends inside the region. Captures what the
  volume does not say: locality, NUMA effects, frequency.

Each is the ratio `average / maximum`: **1 means perfect balance**, and 0.5 means
the busiest thread does twice the average work, so half the machine is lost at
the barriers.

## Usage

    awk -f instrument_omp.awk kernel.pesto.c > kernel.pesto.lb.c
    awk -f instrument_omp.awk kernel.pluto.c > kernel.pluto.lb.c

    gcc -O3 -march=native -fopenmp -DPESTO_LOADBALANCE \
        -I. -I<your includes> kernel.pesto.lb.c -o kernel.pesto.lb -lm

    OMP_NUM_THREADS=64 ./kernel.pesto.lb

Output goes to `stderr`, so it does not interfere with a result dump:

    [loadbalance] regions=400  mean_points=4010000  balance_volume=0.4251  balance_time=0.4263

Without `-DPESTO_LOADBALANCE`, every macro reduces to `do {} while (0)`: the
instrumented file is then strictly equivalent to the original and can serve as
the single source.

## Fitting it into a search campaign

Pluto codes are regenerated at every tile size, so the instrumentation has to be
reapplied each time. In the search scripts, insert the call between generation
and compilation:

    ${POLYCC} --tile --parallel ... ${CODENAME}.c -o ${CODENAME}.pluto.c
    sed -i "s/pragma omp \(.*\)/pragma omp \1 schedule(static)/g" ${CODENAME}.pluto.c
    awk -f instrument_omp.awk ${CODENAME}.pluto.c > ${CODENAME}.pluto.lb.c
    gcc ... -DPESTO_LOADBALANCE ${CODENAME}.pluto.lb.c -o ${CODENAME}.pluto

## Measuring time cleanly

Counting points adds one increment per iteration of the innermost loop. For an
undisturbed time measurement, recompile with

    -DPESTO_LOADBALANCE -DPESTO_LB_TIME_ONLY

which disables the volume counter and keeps only the timers. Two runs are
therefore needed if both indicators are wanted.

## Reading the results

Comparing PESTO under `static` with Pluto under `static` compares the balance
each tiling **builds**, and is the comparison that carries the argument.

Comparing Pluto under `static` with Pluto under `dynamic` or `guided` measures
what the OpenMP scheduler recovers, hence what the initial imbalance costs when
left to run time.

Comparing PESTO under `static` with Pluto under `dynamic` mixes the two effects
and supports no conclusion.

## What the script relies on

Three regularities of both generators, worth rechecking should their output ever
change:

- a parallel region is a `#pragma omp parallel for`, possibly continued over
  several lines with `\`, followed by the loop it distributes;
- an iteration point is an entry into the body of a loop that contains no other
  loop — the file is read whole and each `for` matched to its closing brace,
  which avoids counting the intermediate levels as well;
- the measurement variables are declared inside the body of the parallel loop
  and are therefore private by construction: the existing `private` and
  `firstprivate` clauses are left untouched.

The thread count is read at run time with `omp_get_num_threads()`, which covers
the `num_threads()` clause PESTO emits and stays correct when a thread receives
no iteration under `dynamic`.

## Known limits

- Work performed **outside** a parallel region is not counted: the indicator is
  about the distribution among threads, not about the sequential fraction.
- The final report is placed before the first `return` of `main`. If the kernel
  has another structure, the script says so in a comment at the end of the file
  and the call to `__lb_report()` has to be placed by hand.
- A region whose bounds are all empty is skipped: it says nothing about balance
  and would distort the average over regions.
