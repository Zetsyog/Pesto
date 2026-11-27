# Testing

Pesto's correctness tests are differential: a program and its transformed version
are compiled with the same flags, run on the same inputs, and their full result
dumps must be byte-identical. Everything runs through CTest: CMake registers the
cases, and each case runs a Python script from `test/harness/` (Python >= 3.10,
standard library only; `gen_maxcut.py` additionally needs `sympy`).

| Suite | CTest label | Content | Typical time |
| --- | --- | --- | --- |
| [Unit tests](#unit-tests) | `workflow` | GTest, `test/GTest/` | < 1 s |
| [Workflow](#workflow) | `workflow` | CLI behaviour: default and JSON pipelines, errors, concurrent runs | seconds |
| [Fixture regressions](#fixture-regressions) | `regression` | small synthetic kernels, `test/regression/fixtures/` | under a minute |
| [Benchmark regressions](#benchmark-regression-cases) | `regression;bench` | the tested PolyBench/NPDP (kernel, pipeline) pairs of `benchmarks.txt` | minutes |
| [Benchmark probe](#benchmark-probe) | (own directory) | every PolyBench/NPDP kernel through every pipeline of its class | ~10 min on 12 cores |

`check-regressions` runs the unit, workflow, fixture, and benchmark suites; it is
the CI merge gate. The probe is a report, not a gate: it runs on scheduled pipelines.

## Running the tests

Configure once:

```bash
cmake -S . -B build
```

The `test/` directory is excluded from the default build, so a plain
`cmake --build build` does not build the unit-test binary. Use the `check-*`
targets below, which build what they need first. Running `ctest` directly
without them reports the GTest cases as "Not Run". Build them with
`cmake --build build --target pesto_gtest` first.

All `ctest` commands below can also run from inside `build/` without
`--test-dir build`. Add `--output-on-failure` to see why a case failed, and
`--parallel <n>` (or `-j<n>`) to run cases concurrently.

### Everything (CI gate)

```bash
cmake --build build --target check-regressions -j8
```

This runs every case labelled `workflow` or `regression` (unit, workflow, fixture,
and benchmark suites) with `--parallel 8`. It does not run the probe.

### Unit tests

```bash
cmake --build build --target unit-tests          # builds and runs pesto_gtest directly
ctest --test-dir build -L workflow -E '^workflow-'   # the same tests, through CTest
```

The GTest cases are named `<Suite>.<Test>`, for example
`ParamConstraint.IslExpression`. To run one:

```bash
ctest --test-dir build --output-on-failure -R '^ParamConstraint\.IslExpression$'
```

### Workflow

```bash
cmake --build build --target check-workflow      # also runs the unit tests (same label)
ctest --test-dir build --output-on-failure -R '^workflow-'   # workflow cases only
```

The cases are `workflow-default`, `-json`, `-skew`, `-errors`, and `-concurrent`.

### Fixture regressions

```bash
ctest --test-dir build --output-on-failure -L regression -LE bench --parallel 8
```

Cases are named `regression-<fixture>-<pipeline>[-<tile>]`, for example
`regression-triangle-algebraic`, plus `regression-rejections` (inputs pesto must
reject). To run one fixture:

```bash
ctest --test-dir build --output-on-failure -R '^regression-triangle-'
```

Artifacts are kept in `build/test/regression/cases/<test-name>/`.

### Benchmark regression cases

```bash
ctest --test-dir build --output-on-failure -L bench --parallel 8
```

Cases are named `bench-<suite>-<kernel>-<pipeline>`. Select them with a regex:

```bash
ctest --test-dir build --output-on-failure -R '^bench-polybench-gemm-algebraic$'  # one case
ctest --test-dir build --output-on-failure -R '^bench-polybench-3mm-'             # one kernel
ctest --test-dir build --output-on-failure -L bench -R 'tpz$'                     # one pipeline
ctest --test-dir build --output-on-failure -R '^bench-npdp-'                      # one suite
```

Artifacts are kept in `build/test/regression/cases/<test-name>/`. The sources are
fetched at configure time (see [Sources](#sources)). `pluto` cases exist only when
`PESTO_BENCH_POLYCC` is set (see [Pluto reference](#pluto-reference)).

### Benchmark probe

```bash
cmake --build build --target check-bench-probe
```

See [The probe](#the-probe) for the report it writes and how to rerun a subset.

### Options that change what runs

| CMake option | Effect |
| --- | --- |
| `-DPESTO_REGRESSION_SANITIZERS=OFF` | compile baseline and generated C without ASan/UBSan |
| `-DPESTO_ENABLE_BENCH_REGRESSIONS=OFF` | drop the benchmark regressions and the probe |
| `-DPESTO_BENCH_POLYCC=<polycc command>` | add the `pluto` reference pipeline |
| `-DPESTO_REGRESSION_ARTIFACT_DIR=<dir>` | where fixture and benchmark cases keep their artifacts |
| `-DPESTO_ENABLE_SANITIZERS=ON` | instrument pesto itself with ASan/UBSan (use with `CMAKE_BUILD_TYPE=Debug`) |

### Replaying a failure by hand

`ctest --test-dir build -V -R '^<name>$'` prints the exact harness command of a
case. Each work directory holds `commands.log` (every command the case ran),
`<phase>.out` and `<phase>.err`, and `result.txt` (the classified outcome, see
[The harness](#the-harness)).

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
  benchmarks.txt       the benchmark table: every kernel, its class, its tested pipelines
  benchmarks.cmake     source fetches, class pipelines, ctest cases and probe from the table
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
| Instances | `MEDIUM_DATASET` | `N = 500` x `NPDP_RSEED = 1, 7` |
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
| Parameter tiling | uneven `(3,5,7,11)`, oversized `(31,33,35,37)` for `DIV0..DIV3` |
| Trapezoidal tiling | `(DIV0, DIV2)` = `(2,4)`, `(3,5)`, `(64,67)`; `DIV1` keeps its default |

The trapezoidal ranges follow the pesto-bench performance runs (`DIV0 >= 2`,
`DIV2 >= 4`); smaller values are outside what TpzTiling supports.

With large dividers, a slice can be thinner than one column of the level it
cuts. Both of its ends then land on the same column, giving `lb == ub + 1`: the
slice is empty, and the next one starts at that column, so the partition stays
exact. The generated bound check (`boundCorrectionStrategy`) accepts this and only
fires on `lb > ub + 1`, which means the bounds are wrong.

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
3. instantiates the domain: substitutes the values for the parameters
   (`4_PB_TSTEPS` becomes `4*100`) and empties the parameter list (`[] -> { ... }`).
   The constraints left without variables (pesto's parameter context, such as
   `_PB_N >= 0`) are checked, then dropped: an instance outside that context is
   a `maxcut-fail`. A concrete domain also keeps `lexmin(x1)` from being
   piecewise in a parameter;
4. runs `utilities/gen_maxcut.py` (Python 3, `sympy`, barvinok's `iscc` from the
   build) with `--func max_cut_volume`. Without parameters it emits
   `max_cut_volume(long b)`, the function the generated code calls.

`maxcut.h` is regenerated for every `DIV0` (one `maxcut-<instance>-t<t>` phase
each), since `--t` depends on it.

### Pipelines and kernel classes

| Pipeline | Configuration |
| --- | --- |
| `default` | no `--config`: CLooG regenerates the input, without Pluto |
| `pluto-like` | Pluto tiling (tile size 7 for benchmarks) + CLooG |
| `algebraic` | Pluto + algebraic parameter tiling |
| `hybrid` | Pluto + algebraic/rectangular parameter tiling |
| `tpz` | Pluto diamond tiling + TpzTiling |
| `handtuned` | the hand-tuned configuration `test/regression/pipelines/handtuned/<suite>/<kernel>.json` |
| `pluto` | Pluto's `polycc --tile --parallel --nounroll` (reference, probe only) |

Which pipelines the probe runs depends on the kernel's class, given for each kernel
in `test/regression/benchmarks.txt`:

| Class | Probed pipelines |
| --- | --- |
| `algebraic` | `default`, `pluto-like`, `algebraic`, `hybrid` |
| `trapezoidal` (PolyBench) | `default`, `pluto-like`, `tpz`, `handtuned` (if present) |
| `trapezoidal` (NPDP) | `default`, `pluto-like`, `handtuned` |
| `unsupported` (not pesto-compatible) | none (only `pluto`) |

`pluto` is added to every class when `PESTO_BENCH_POLYCC` is set. These lists are
the `PESTO_BENCH_CLASS_PIPELINES_<suite>_<class>` variables of `benchmarks.cmake`.

Trapezoidal kernels are never valid with algebraic tiling. NPDP kernels need the
skew of their hand-tuned configuration before trapezoidal tiling, so the generic
`tpz` pipeline does not apply to them. NPDP mea, sw3d, and zuker are not tested.

Per-kernel `polycc` flags go in `PESTO_BENCH_POLYCC_FLAGS_<kernel>` (heat-3d uses
`--nocloogbacktrack`).

### Tested pairs (allowlist)

`test/regression/benchmarks.txt` has one row per kernel; its last columns are the
pipelines tested by CTest:

```text
# suite    kernel                              class        tested pipelines
polybench  linear-algebra/blas/gemm            algebraic    default pluto-like algebraic hybrid
polybench  linear-algebra/kernels/bicg         algebraic    pluto-like algebraic hybrid  # default: ...
```

The first row creates `bench-polybench-gemm-default`, `bench-polybench-gemm-pluto-like`,
and so on. A probed pipeline left out of a row has its reason in the trailing
comment; `-` means no tested pipeline. Configuring fails with a clear message on a
malformed row, an unknown class, or a tested pipeline the probe does not run for
that kernel. Editing the file re-runs CMake on the next build. Replay one case with:

```bash
ctest --test-dir build --output-on-failure -R '^bench-polybench-gemm-algebraic$'
# artifacts: build/test/regression/cases/bench-polybench-gemm-algebraic/
```

## The probe

```bash
cmake --build build --target check-bench-probe
```

runs every kernel through every pipeline of its class (roughly 10 minutes on 12
cores), in parallel, from `build/bench-probe/` — a separate
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
- **A benchmark kernel**: add a row to `test/regression/benchmarks.txt`.
- **A pipeline**: add `pipelines/<name>.json.in`, then list it in the
  `PESTO_BENCH_CLASS_PIPELINES_*` variables of `benchmarks.cmake`.
- **A hand-tuned configuration**: add
  `pipelines/handtuned/{polybench,npdp}/<kernel>.json`; the probe picks it up as
  the `handtuned` pipeline for trapezoidal kernels.
- **A tested pair**: run the probe, then add the pipeline to the kernel's row in
  `benchmarks.txt` and drop it from the row's comment.

## Known issues (probe of 2026-11-27)

161 of 177 probe cases pass. Each remaining failure is left out of the tested
pipelines, with its reason in `benchmarks.txt`.

Pesto:

- PolyBench durbin: `pluto-like`, `algebraic`, and `hybrid` give wrong results
  with 2 or 4 threads. `default` (no Pluto) and Pluto's own `polycc` output are
  correct, so the parallelization through pesto is wrong.
- TpzTiling aborts with "Trapezoidal tiling cannot be performed on this scop" on
  PolyBench nussinov and trisolv (`tpz`) and NPDP triang (`handtuned`).
- PolyBench ludcmp: `algebraic` and `hybrid` end `pass-untiled` (parameter tiling
  finds nothing to tile).

Configurations and sources:

- PolyBench trisolv (`handtuned`): the hand-tuned skew names `j`, which is not an
  iterator of every statement of the upstream source.
- PolyBench adi: clan cannot parse `(DATA_TYPE)_PB_N`, so every pipeline fails,
  Pluto's own included.

Toolchain:

- nussinov, both PolyBench and NPDP: Pluto's own output gives wrong results with
  2 threads. Pesto's pipelines pass on both.

Solved since the 2026-11-26 probe:

- Algebraic tiling on 2mm, 3mm, and gramschmidt: tiling data shared between
  statement groups, empty slices, and the volume check on empty slices.
- The 3mm transform timeout: CLooG now uses isl's Fourier-Motzkin convex hull.
- Scattering dimensions shadowing program variables (deriche `b1`, `b2`).
- Wrong results with trapezoidal tiling on lu (`tpz`, `handtuned`) and NPDP
  counting_im, mcm, nw, and sw (`handtuned`). NPDP mcc (`handtuned`) passes too
  (not re-checked whether its output is tiled now).
- The 4-thread races of the old Pluto-based `default` pipeline on bicg and NPDP mcm,
  nw, sw, and triang no longer reproduce. They were intermittent, so a few passing
  probe runs do not prove them gone.
- The `max_cut_volume()` signature mismatch: the harness instantiates the domain
  before calling `gen_maxcut.py`, which then emits the function the generated code
  calls.
