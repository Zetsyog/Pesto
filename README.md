# Pesto &#x1F90C;

[![pipeline status](https://gitlab.inria.fr/crossett/pesto/badges/main/pipeline.svg?key_text=Build+and+Test+(main)&key_width=150)](https://gitlab.inria.fr/crossett/pesto/-/commits/main)

[![pipeline status](https://gitlab.inria.fr/crossett/pesto/badges/dev/pipeline.svg?key_text=Build+and+Test+(dev)&key_width=150)](https://gitlab.inria.fr/crossett/pesto/-/commits/dev)


Polyhedral flExible loop-neST Optimizer (PESTO)

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
  - MPC
  - llvm/clang (max version is llvm14)
  - Bison/Flex library
- Trahrhe dependencies:
  - A working [Barvinok](https://barvinok.sourceforge.io/) installation (for trahrhe software)
  - Maxima

The same list as Debian-like package list:

```bash
sudo apt install bash cmake gcc g++ git make pkgconf libtool autoconf automake libyaml-dev libgmp3-dev llvm-14 llvm-14-dev libclang-14-dev bison libbison-dev libfl-dev mpc libmpc-dev libntl-dev maxima
```

Then you will need to install barvinok by source.

### Build on your own machine

Run the following commands to create a pesto executable.
Note: the `install` command is untested and can be unpredictable.

The supported CMake options are the following:

- `PESTO_BUILD_JOBS`: define the number of threads/jobs that will be used to build subprojects (i.e. it will call `make -j ${PESTO_BUILD_JOBS})
- `PESTO_TRAHRHE_VERSION`: specify trahrhe version to use (default is `4.2`)
- `PESTO_TEST_POLYBENCH_DATASET`: for PolyBench tests, set which dataset will be used
- `PESTO_TEST_POLYBENCH_TILE_DIVIDER_MIN`: for PolyBench tests with random tile sizes, set the minimum tile divider/size
- `PESTO_TEST_POLYBENCH_TILE_DIVIDER_MAX`: for PolyBench tests with random tile sizes, set the maximum tile divider/size

```bash
mkdir build
cd build
cmake .. -DPESTO_BUILD_JOBS=16
cmake --build . -j
# make install # not implemented
# now you can call cli with
./pesto [args]
```

### Build using Docker

To build the container run the following command:

```bash
docker build . -t pesto:latest -f utils/Dockerfile --build-arg "repository_dir=./"
```

Then you can start a shell session in this container with the current host dir mounted as `/mnt` like this:

```bash
docker run -it --rm -v "$(pwd):/mnt" -w /mnt pesto:latest bash
```

Then inside the container you can call pesto with

```bash
$PESTO --config <path to json config> <input.c> -o <output.c>
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
            pkgs.gmp
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
              libyaml
              gmp
              cmake
              autoconf
              automake
              pkgconf
              libtool
              llvmPackages_14.libllvm
              llvmPackages_14.clang
              llvmPackages_14.lld
              llvmPackages_14.libclang
              llvmPackages_14.compiler-rt
              llvmPackages_14.lldb
              llvmPackages_14.openmp
              bison
              flex
              # trahrhe
              ntl
              maxima
              libmpc
              #misc
              bc
            ]
            ++ [ barvinok ];
          shellHook = ''
            export PESTO="$PWD/build/pesto"
          '';
        };
      }
    );
}

```

## CLI interface

Using cli (*TODO*)

## JSON interface

You can pass a json config file to the CLI interface.

```bash
pesto --config opt.json 2mm.c -o 2mm.pesto.c
```

### opt.json

```json
{
    "options": {
        "input-format": "c"
    },
    "passes": [
        {
            "uuid": "pluto",
            "options": {
                "tile": false,
                "parallel": false,
                "prevector": false,
                "silent": true
            }
        },
        {
            "uuid": "split-nested-loops"
        },
        {
            "uuid": "param-tiling",
            "gen-trahrhe": true,
            "trahrhe-mask": "d",
            "pluto": {
                "options": {
                    "tile": true,
                    "parallel": true,
                    "prevector": true,
                    "intratileopt": true
                }
            }
        },
        {
            "uuid": "clast-custom-vec-pragma",
            "pragma": "GCC ivdep"
        },
        {
            "uuid": "codegen-cloog"
        }
    ]
}
```

## Test (wip)

```bash
cmake --build . --target Pesto_check
```
