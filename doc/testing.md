# Testing

Pesto's correctness tests are differential: a program and its transformed version
are compiled with the same flags, run on the same inputs, and their full result
dumps must be byte-identical. Everything runs through CTest: CMake registers the
cases, and each case runs a Python script from `test/harness/` (Python >= 3.10,
standard library only; `gen_maxcut.py` additionally needs `sympy`).

| Suite | Label | Content | Run with |
| --- | --- | --- | --- |
| Unit tests | | GTest, `test/GTest/` | `ctest -R gtest` |
| Workflow | `workflow` | CLI behaviour: default and JSON pipelines, errors, concurrent runs | `check-workflow` |
| Fixture regressions | `regression` | small synthetic kernels, `test/regression/fixtures/` | `check-regressions` |
| Benchmark regressions | `regression;bench` | allowlisted PolyBench/NPDP (kernel, pipeline) pairs | `check-regressions` |
| Benchmark probe | (own directory) | every PolyBench/NPDP kernel through every pipeline that applies | `check-bench-probe` |

`check-regressions` runs the workflow, fixture, and benchmark suites; it is the
CI merge gate. The probe is a report, not a gate: it runs on scheduled pipelines.

## Layout

```text
test/harness/          what a case does at run time (Python)
  harness.py           shared helpers: run a phase, classify failures, hash dumps
  run_fixture.py       one fixture through one pipeline
  run_bench.py         one PolyBench/NPDP kernel through one pipeline
  reject.py            inputs pesto must reject
  workflow.py          CLI behaviour (workflow suite)
  probe_report.py      runs the probe and writes the report
test/regression/       which cases exist (CMake)
  CMakeLists.txt       fixture x pipeline matrix, rejections
  benchmarks.cmake     source fetches, kernel classes, allowlist, probe target
  pipelines/*.json.in  pesto configurations (@TILE@: fixed Pluto tile size)
  pipelines/handtuned/ hand-tuned per-kernel configurations, <suite>/<kernel>.json
  polycc-docker.sh     polycc from docker/pluto.dockerfile (Pluto reference)
  fixtures/            synthetic kernels for run_fixture.py
```

Every script takes its inputs as command-line options (`--help` lists them);
`ctest -V -R <name>` prints the exact command line of a case.

## The harness

Each case owns a work directory. Every command is appended to `commands.log`, and
its output is kept as `<phase>.out` / `<phase>.err`, so a failure can be replayed
by hand. The first failure is classified and written to `result.txt`:

| Status | Meaning |
| --- | --- |
| `transform-fail` | pesto (or polycc) failed, or produced no output |
| `maxcut-fail` | `maxcut.h` generation failed (trapezoidal tiling) |
| `baseline-fail` | the original program did not build or run (harness or source problem) |
| `compile-fail` | the generated code does not compile |
| `crash` | the transformed program exited with an error |
| `sanitizer` | ASan/UBSan report |
| `timeout` | a phase exceeded its limit, or CTest killed the case |
| `mismatch` | the result dump differs from the original's |
| `pass` / `pass-untiled` | identical results; `pass-untiled` means a parameter-tiling pipeline produced no `DIV` macro |
| `harness-error` | the test script itself raised an exception (a harness bug, traceback in the CTest output) |

Each command runs in its own process group; on timeout the whole group is
killed, including processes forked by pesto.
Phases have a 60 s limit (300 s for compiling transformed code, as `maxcut.h` can
reach megabytes).

Code is compiled with `-O2 -fopenmp -ffp-contract=off` plus
`-fsanitize=address,undefined` (disable with `-DPESTO_REGRESSION_SANITIZERS=OFF`).
`-ffp-contract=off` matters: without it, FMA contraction differs between the
original and the transformed loops and floating-point results diverge.

## Benchmark regressions

### Sources

Both suites are fetched at configure time by `benchmarks.cmake`:

- PolyBench/C 4.2.1-beta, the official tarball, pinned by SHA256;
- [NPDP Bench](https://github.com/Zetsyog/NPDP_Bench), pinned commit. Its kernels
  take the size as a parameter `n` of `kernel_<name>(int n, ...)`, so `N` never
  appears in a scop.

With `FETCHCONTENT_FULLY_DISCONNECTED=ON` on a fresh build directory, the sources
are missing: CMake warns and disables the suite. Configure once with
`-DFETCHCONTENT_FULLY_DISCONNECTED=OFF`. `-DPESTO_ENABLE_BENCH_REGRESSIONS=OFF`
disables it.

### What a case checks

| | PolyBench | NPDP |
| --- | --- | --- |
| Instances | `MINI_DATASET`, `SMALL_DATASET` | `N = 10, 23, 64` x `NPDP_RSEED = 1, 7` |
| Dump | `-DPOLYBENCH_DUMP_ARRAYS` (stderr) | `-DNPDP_DUMP` (stderr) |
| Extra sources | `utilities/polybench.c` | headers in `include/` |

For each instance, the transformed program is built with several tile divider
sets and run with 1, 2, and 4 OpenMP threads. Parameter tiling (`algebraic`,
`hybrid`, `tpz`) leaves tile sizes symbolic: pesto never sees them, the generated
code reads them from the `DIVn` macros, set here with `-DDIVn=...` at compile
time. Only `pluto-like` has fixed tile sizes, through `@TILE@`.

| Generated code | Divider sets |
| --- | --- |
| No `DIV` macros | none |
| Parameter tiling | unit `(1,1,1,1)`, uneven `(3,5,7,11)`, oversized `(64,65,66,67)` for `DIV0..DIV3` |
| Trapezoidal tiling | `(DIV0, DIV2)` = `(2,4)`, `(3,5)`, `(64,67)`; `DIV1` keeps its default |

The trapezoidal ranges follow the pesto-bench performance runs (`DIV0 >= 2`,
`DIV2 >= 4`); smaller values are outside what TpzTiling supports.

The non-fatal TpzTiling warning `Warning: target volume too small.` (two lines on
stderr) is removed from dumps before comparison.

### Trapezoidal tiling and `maxcut.h`

TpzTiling output includes `maxcut.h` and documents, in a comment, the
`gen_maxcut.py` command that produces it. For each instance and `DIV0`, the
harness:

1. reads the domain, `--t` (`DIV0` times the band stride) and `--f1` from that
   comment;
2. resolves the domain parameters for the instance: PolyBench `_PB_X` is the
   dataset macro `X` (printed by a small helper program); NPDP `n` is `N` and `l`
   is 0 (`NPDP_MIN_LOOP`);
3. pins the parameters in the domain (`n = 23 and ...`), which also keeps
   `lexmin(x1)` from being piecewise in a parameter;
4. runs `utilities/gen_maxcut.py` (Python 3, `sympy`, barvinok's `iscc` from the
   build) with `--func pesto_maxcut_impl`, and appends
   `static long max_cut_volume(long b)` binding the parameter values.

Step 4 works around a mismatch: the generated code calls `max_cut_volume(b)`,
while `gen_maxcut.py` emits `max_cut_volume(params..., b)`.

### Pipelines and kernel classes

| Pipeline | Configuration |
| --- | --- |
| `default` | no `--config` |
| `pluto-like` | Pluto tiling (tile size 7 for benchmarks) + CLooG |
| `algebraic` | Pluto + algebraic parameter tiling |
| `hybrid` | Pluto + algebraic/rectangular parameter tiling |
| `tpz` | Pluto diamond tiling + TpzTiling |
| `handtuned` | the hand-tuned configuration `test/regression/pipelines/handtuned/<suite>/<kernel>.json` |
| `pluto` | Pluto's `polycc --tile --parallel --nounroll` (reference, probe only) |

Which pipelines apply depends on the kernel:

| Class | Kernels | Pipelines |
| --- | --- | --- |
| Algebraic | PolyBench correlation, covariance, 2mm, 3mm, atax, bicg, doitgen, mvt, gemm, gemver, gesummv, syr2k, syrk, trmm, gramschmidt, deriche | `default`, `pluto-like`, `algebraic`, `hybrid` |
| Trapezoidal | PolyBench lu, cholesky, nussinov, seidel-2d, fdtd-2d, symm, heat-3d, jacobi-1d, jacobi-2d, trisolv | `default`, `pluto-like`, `tpz`, `handtuned` (if present) |
| Trapezoidal | NPDP counting, counting_im, mcm, knuth, triang, nussinov, mcc, nw, sw | `default`, `pluto-like`, `handtuned` |
| Not pesto-compatible | PolyBench floyd-warshall, adi, ludcmp, durbin | `pluto` only |

Trapezoidal kernels are never valid with algebraic tiling. NPDP kernels need the
skew of their hand-tuned configuration before trapezoidal tiling, so the generic
`tpz` pipeline does not apply to them. NPDP mea, sw3d, and zuker are not tested.

Per-kernel `polycc` flags go in `PESTO_BENCH_POLYCC_FLAGS_<kernel>` (heat-3d uses
`--nocloogbacktrack`).

### Allowlist

The bottom of `test/regression/benchmarks.cmake` registers the CTest cases:

```cmake
pesto_bench(polybench linear-algebra/blas/gemm PIPELINES default pluto-like algebraic hybrid)
```

creates `bench-polybench-gemm-default`, `bench-polybench-gemm-pluto-like`, and so on.
Every excluded pair has its reason in a comment. Replay one case with:

```bash
ctest --test-dir build --output-on-failure -R '^bench-polybench-gemm-algebraic$'
# artifacts: build/test/regression/cases/bench-polybench-gemm-algebraic/
```

## The probe

```bash
cmake --build build --target check-bench-probe
```

runs every kernel through every pipeline that applies (about 130 cases, roughly
10 minutes on 12 cores), in parallel, from `build/bench-probe/` — a separate
CTest directory, so a plain `ctest` never runs it. It writes:

- `build/bench-probe/probe-report.md`: a kernel x pipeline table of statuses,
  allowlisted cells in bold, then the passing pairs that are not allowlisted yet,
  and the allowlisted pairs that fail;
- `build/bench-probe/probe-report.csv`;
- `build/bench-probe/cases/<suite>/<kernel>/<pipeline>/`: each case's artifacts.

The target fails when an allowlisted pair fails. To rerun a subset, then refresh
the report without running everything:

```bash
ctest --test-dir build/bench-probe -R 'seidel-2d' --parallel 4
python3 test/harness/probe_report.py --probe-dir build/bench-probe --report-only
```

### Pluto reference

The `pluto` column runs the same checks on Pluto's own output, to tell pesto bugs
from kernels the polyhedral toolchain cannot handle. Pesto builds libpluto
without `polycc`, so the probe uses the image from `docker/pluto.dockerfile`:

```bash
docker build -t pesto-pluto-ref:0.13.0 -f docker/pluto.dockerfile docker
cmake -B build -DPESTO_BENCH_POLYCC=$PWD/test/regression/polycc-docker.sh
```

`polycc-docker.sh` mounts the current directory at the same path and assumes
rootless Docker (files written by the container then belong to the caller).
`PESTO_PLUTO_IMAGE` selects another image.

## Adding things

- **A fixture**: add `test/regression/fixtures/<name>.c` (see `fixture.h`: `argv[1]` is the size,
  `argv[2]` the seed, results on stdout) and a `pesto_regression()` line in
  `test/regression/CMakeLists.txt`: `pesto_regression(<fixture> <pipeline>
  [TILE <size>])`, where `TILE` only applies to pipelines with `@TILE@`.
  Fixtures run parameter-tiled code with unit `(1,1,1)`, uneven `(3,5,7)`, and
  oversized `(64,65,66)` `DIV0..DIV2`.
- **A pipeline**: add `pipelines/<name>.json.in`, then list it in the kernel
  class variables of `benchmarks.cmake`.
- **A hand-tuned configuration**: add
  `pipelines/handtuned/{polybench,npdp}/<kernel>.json`; the probe picks it up as
  the `handtuned` pipeline for trapezoidal kernels.
- **An allowlist entry**: run the probe, then move the pair from the excluded
  comments to a `pesto_bench()` call.

## Known issues (probe of 2026-11-26)

Found while setting up the suite; each one is excluded from the allowlist with
its reason.

Pesto:

- Pluto scheduling through pesto (`default` pipeline) gives wrong results with 4
  threads on NPDP mcm, nw, sw, triang and PolyBench durbin and bicg. Pluto's own
  `polycc` output is correct on all of them, so the race comes from pesto's path.
  In bicg, the `i` loop is marked `omp parallel for` while `s[j]` accumulates over
  `i`. These races are intermittent: a single probe run can miss them (bicg passed
  the probe and failed the next run).
- Algebraic tiling: 2mm, 3mm, gramschmidt (`algebraic`) abort; 2mm (`hybrid`)
  crashes at run time; 3mm (`hybrid`) gives wrong results.
- Trapezoidal tiling, wrong results: lu, cholesky (even with 1 thread), PolyBench
  nussinov (`handtuned`); NPDP counting_im, mcm, nw, sw, triang (mostly with 2+
  threads).
- PolyBench nussinov and trisolv (`tpz`): pesto aborts.
- NPDP mcc (`handtuned`): TpzTiling logs "No for loop found to tile" as an error but
  pesto exits 0; the skewed, untiled output is already wrong.
- `max_cut_volume()`: signature mismatch between the generated call and
  `gen_maxcut.py` (worked around by the harness, see above).

Configurations and sources:

- PolyBench trisolv (`handtuned`): the hand-tuned skew names `j`, which is not an
  iterator of every statement of the upstream source.
- PolyBench adi: clan cannot parse `(DATA_TYPE)_PB_N`; Pluto fails the same way.

Toolchain:

- PolyBench nussinov: Pluto's own output also gives wrong results (2 threads).
