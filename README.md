# Pesto &#x1F90C;

[![pipeline status](https://gitlab.inria.fr/crossett/pesto/badges/main/pipeline.svg?key_text=Build+and+Test+(main)&key_width=150)](https://gitlab.inria.fr/crossett/pesto/-/commits/main)

Polyhedral Extensible loop-neST Optimizer (PESTO)

```bash
./pesto <input_file> [-h,--help] [-v,--version]
```

This tool allows to easily apply polyhedral transformation on C code.

It is divided in two parts : the command-line interface, and the library.

## Build

### Dependencies

This need to build [pluto](https://github.com/bondhugula/pluto) and [trahrhe](https://webpages.gitlabpages.inria.fr/trahrhe/about) software.
CMake is also required.

Here is a list of the required packages:

- A working c/c++ compiler
- CMake
- Pluto dependencies
  - Autotools software collections
  - GMP lib
  - llvm/clang (max version is llvm14)
  - Bison/Flex library
- Trahrhe dependencies:
  - A working [Barvinok](https://barvinok.sourceforge.io/) installation (for trahrhe software)
  - Maxima

The same list as Debian-like package list:

```bash
sudo apt install bash cmake gcc g++ git make pkgconf libtool autoconf automake libyaml-dev libgmp3-dev llvm-14 llvm-14-dev libclang-14-dev bison libbison-dev libfl-dev libntl-dev maxima
```

Then you will need to install barvinok by source.

### Build on your own machine

Run the following commands to create a pesto executable.
Note: the `install` command is untested and can be unpredictable.

The supported CMake options are the following:

- `PESTO_BUILD_JOBS`: define the number of threads/jobs that will be used to build subprojects (i.e. it will call `make -j ${PESTO_BUILD_JOBS})
- `PESTO_ENABLE_BENCH_REGRESSIONS` (default `ON`): run PolyBench and NPDP kernels as regression tests (fetches their pinned sources)
- `PESTO_BENCH_POLYCC`: `polycc` command for the benchmark probe's Pluto reference column (empty: disabled)
- `PESTO_ENABLE_BENCHMARKS` (default `ON`): fetch the pesto-bench performance repository

```bash
mkdir build
cd build
cmake .. -DPESTO_BUILD_JOBS=16
cmake --build . -j
# make install # not implemented
# now you can call cli with
./cli/pesto [args]
```

### Build using Docker

To build the container run the following command:

```bash
docker build . -t pesto:latest -f docker/Dockerfile --build-arg "repository_dir=./"
```

Then you can start a shell session in this container with the current host dir mounted as `/mnt` like this:

```bash
docker run -it --rm -v "$(pwd):/mnt" -w /mnt pesto:latest bash
```

Then inside the container you can call pesto with

```bash
$PESTO --config <path to json config> <input.c> -o <output.c>
```

Build the ci container

```bash
docker build . -t pesto-ci:latest -f docker/ci.dockerfile 
```

### Build using nix

Here is a flake.nix file that will allow you to build pesto.

flake.nix

```nix
{
  description = "A flake for using Pesto";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  inputs.systems.url = "github:nix-systems/default";
  inputs.flake-utils = {
    url = "github:numtide/flake-utils";
    inputs.systems.follows = "systems";
  };

  outputs =
    { nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        barvinok = pkgs.stdenv.mkDerivation rec {
          pname = "barvinok";
          version = "0.41.8";
          src = pkgs.fetchurl {
            url = "https://barvinok.sourceforge.io/barvinok-${version}.tar.gz";
            sha256 = "sha256-zED+7lqILqn7ruaWswPzd+nEayMhgxMDSAvtGtuOyjo=";
          };
          buildInputs = [
            pkgs.stdenv.cc.cc.lib
            pkgs.gmp
            pkgs.gcc
            pkgs.ntl
            pkgs.libtool
            pkgs.automake
            pkgs.autoconf
            pkgs.bison
            pkgs.flex
          ];
          nativeBuildInputs = [ pkgs.pkgconf ];
          configurePhase = ''
            ./configure --prefix=$out
          '';
          buildPhase = ''
            make -j
          '';
          installPhase = ''
            make install
          '';
        };
      in
      {
        devShells.default = pkgs.mkShell {
          packages =
            with pkgs;
            [
              git
              gcc
              libyaml
              gmp
              cmake
              autoconf
              automake
              pkgconf
              libtool
              bison
              flex
              # trahrhe
              ntl
              maxima
              libmpc
              bc
            ]
            ++ [ barvinok ];
          shellHook = ''
            export PESTO="$PWD/build/pesto"
            export LD_LIBRARY_PATH="${pkgs.stdenv.cc.cc.lib}/lib:$LD_LIBRARY_PATH"
          '';
        };
      }
    );
}

```

## CLI interface

After building, the executable is `build/cli/pesto`. With no configuration file,
PESTO applies Pluto transformations and generates C through CLooG:

```bash
./build/cli/pesto test/workflow/kernel.c -o /tmp/kernel.pesto.c
gcc -O2 -fopenmp test/workflow/kernel.c -o /tmp/kernel.baseline -lm
gcc -O2 -fopenmp /tmp/kernel.pesto.c -o /tmp/kernel.transformed -lm
/tmp/kernel.baseline > /tmp/kernel.expected
OMP_NUM_THREADS=2 /tmp/kernel.transformed > /tmp/kernel.actual
cmp /tmp/kernel.expected /tmp/kernel.actual
```

The equivalent JSON pipeline is in `test/workflow/default.json`:

```bash
./build/cli/pesto --config test/workflow/default.json test/workflow/kernel.c -o /tmp/kernel.pesto.c
```

This source-to-source workflow accepts one `#pragma scop` / `#pragma endscop`
region per C input file. Input and output must be different files, and the output
directory must already exist. Generated iterators have their own block scope.
Intermediate code is isolated per invocation and removed when the bundle is
destroyed; the final source is published only after it has been written successfully.
Failures return a nonzero exit status. `--readscop` remains available for analysis
and dump pipelines; assembling a complete C source requires the original C input.

### JSON configuration

You can pass a json config file to the CLI interface.

```bash
pesto --config opt.json 2mm.c -o 2mm.pesto.c
```

### opt.json

```json
{
    "passes": [
        { "uuid": "pluto" },
        { "uuid": "codegen-cloog" }
    ]
}
```

## Test (wip)

Run the fast workflow regressions (builds the CLI and unit tests first):

```bash
cmake -S . -B build
cmake --build build --target check-workflow -j4
```

These checks exercise default and JSON pipelines, skew pass integration, generated
code compilation and full output comparison at sizes 1, 7, and 24 with one and two
threads, concurrent invocations, and error handling. Once built, they can also run
from the build root with `ctest --test-dir build --output-on-failure -L workflow`.

The compiler regression suite extends these smoke checks with small, deterministic
kernels, rather than performance benchmarks:

```bash
cmake --build build --target check-regressions -j4
```

It checks empty and tiny domains, negative and shifted bounds, triangular domains,
scalar temporaries, reductions with initialization and finalization, guarded loops,
multiple statements and nests, and a time-dependent stencil. The matrix covers
the default pipeline, Pluto tiles of size 3 and 64, rectangular parameter tiling,
hybrid tiling, and algebraic tiling. Parameter tiling is checked with unit,
uneven `(3,5,7)`, and oversized `(64,65,66)` dividers. Every generated program is
compared against the original at sizes `0,1,2,3,4,7,8,9,17`, with two fixed input
patterns and `1,2,4` OpenMP threads. Full result dumps include untouched borders;
the fixtures use bounded integer arithmetic so equality is exact.

Baseline and generated C are compiled with AddressSanitizer and
UndefinedBehaviorSanitizer by default. Unsupported toolchains can explicitly use
`-DPESTO_REGRESSION_SANITIZERS=OFF`. To instrument PESTO itself as well, enable:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DPESTO_ENABLE_SANITIZERS=ON
cmake --build build --target check-regressions -j4
```

This adds ASan/UBSan to PESTO's own C/C++ targets, keeps assertions enabled, and
enables libstdc++ assertions without changing its ABI. External libraries retain
their own build flags. LeakSanitizer is disabled: this suite checks memory-access
safety and undefined behavior, rather than leaks in the dependency stack.
Restore the ordinary build with
`-DPESTO_ENABLE_SANITIZERS=OFF`.

The synthetic suite has 47 CTest entries including the workflow tests and usually
takes tens of seconds after building. The target uses a CTest processor budget of 8;
individual matrix tests reserve 4 for their OpenMP runs. To replay a failure:

```bash
ctest --test-dir build --output-on-failure -R '^regression-triangle-algebraic$'
```

Inputs, generated code and headers, configurations, command logs, diagnostics,
and result dumps remain in `build/test/regression/cases/<test-name>/`. Set
`PESTO_REGRESSION_ARTIFACT_DIR` to change that directory. The CI job retains these
artifacts when a test fails. No random sampling or timing thresholds are used.

The strided-loop fixture goes directly through CLooG; Pluto's importer cannot
handle its local dimensions and has a separate diagnostic regression. Nonpositive
tile sizes and multiple SCoP regions must also fail cleanly.

### Benchmark regressions (PolyBench and NPDP)

Details, kernel classes, and the known issues are in [doc/testing.md](doc/testing.md).

`check-regressions` also runs real kernels: [PolyBench/C 4.2.1](https://sourceforge.net/projects/polybench/)
(official tarball, SHA256-pinned) and [NPDP Bench](https://github.com/Zetsyog/NPDP_Bench)
(pinned commit), both fetched at configure time. Each allowlisted (kernel, pipeline)
pair is a CTest case labelled `bench` (`ctest --test-dir build -L bench`). The
original and the transformed kernel are compiled with the same flags (ASan/UBSan,
`-ffp-contract=off`), and their result dumps must be identical:

- PolyBench at `MINI` and `SMALL` datasets, NPDP at `N = 10, 23, 64` with two seeds;
- three tile divider sets (TPZ: `DIV0 >= 2`, `DIV2 >= 4`, as in the performance runs);
- `1, 2, 4` OpenMP threads.

For trapezoidal tiling the harness generates `maxcut.h` with
`utilities/gen_maxcut.py` (needs Python 3, `sympy`, and barvinok's `iscc`, which
the build provides), from the command emitted in the generated source, with the
parameters bound to the instance being run.

Pipelines live in `test/regression/pipelines/`; `kernel` is the tailored
configuration in `utilities/configuration files/{polybenchs,NPDP}/<kernel>.json`.
Which pipelines apply depends on the kernel:

| Class | Kernels | Pipelines |
| --- | --- | --- |
| Algebraic | the other PolyBench kernels | `default`, `tiled`, `atiling`, `hybrid-2d` |
| Trapezoidal | PolyBench lu, cholesky, nussinov, seidel-2d, fdtd-2d, symm, heat-3d, jacobi-1d, jacobi-2d, trisolv; NPDP counting, counting_im, mcm, knuth, triang, nussinov, mcc, nw, sw | `default`, `tiled`, `tpz` (PolyBench), `kernel` |
| Not pesto-compatible | floyd-warshall, adi, ludcmp, durbin | Pluto reference only |

NPDP mea, sw3d, and zuker are not tested.

The allowlist in `test/regression/benchmarks.cmake` holds the pairs that pass,
with the reason for every excluded pair. To review it, run the probe: every
kernel through every pipeline that applies, in a separate CTest directory that a
plain `ctest` never runs:

```bash
cmake --build build --target check-bench-probe
# report: build/bench-probe/probe-report.md (and .csv)
```

The report lists passing pairs that are not allowlisted yet, and fails if an
allowlisted pair fails. CI runs it on scheduled pipelines. A Pluto reference
column helps tell pesto bugs from kernels the toolchain cannot handle:

```bash
docker build -t pesto-pluto-ref:0.13.0 -f docker/pluto.dockerfile docker
cmake -B build -DPESTO_BENCH_POLYCC=$PWD/test/regression/polycc-docker.sh
```

## Build for development

```bash
cmake -B build -DPESTO_BUILD_JOBS=16 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j
```
