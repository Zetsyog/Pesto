{
  description = "pesto development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    { nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        llvm = pkgs.llvmPackages_15;

        ntl = llvm.stdenv.mkDerivation rec {
          pname = "ntl";
          version = "11.5.1";
          src = pkgs.fetchurl {
            url = "https://www.shoup.net/ntl/ntl-${version}.tar.gz";
            sha256 = "sha256-IQ0GwxMGy8bq9oFEU8Vsd22djo3zbXTrMG9qUj0caoo=";
          };
          buildInputs = [ pkgs.gmp ];
          nativeBuildInputs = [ pkgs.perl ]; # needed for ./configure
          sourceRoot = "${pname}-${version}/src";

          enableParallelBuilding = true;

          dontAddPrefix = true; # DEF_PREFIX instead
          configurePlatforms = [ ];
          configurePhase = ''
            ./configure PREFIX=$out NATIVE=off NTL_GMP_LIP=on CXX=${llvm.stdenv.cc.targetPrefix}c++
          '';

          doCheck = false;
        };
      in
      {
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            git
            gcc

            llvm.clang-tools
            llvm.libllvm
            llvm.lld
            llvm.libclang
            llvm.libcxx
            llvm.openmp

            gdb
            valgrind

            pkgconf
            cmake
            autoconf
            automake
            libtool
            bison
            flex

            # trahrhe
            libyaml
            gmp
            ntl
            maxima
            libmpc
            cddlib
            glpk

            shfmt
            bc
            cmake-format
            likwid

            gitlab-ci-local

            gtest

            # utilities/gen_maxcut.py
            (python3.withPackages (ps: [ ps.sympy ]))
          ];

          shellHook = ''
            export PESTO="$PWD/build/cli/pesto"
            export ROOT_DIR="$PWD/test/pesto-bench/"
            export LD_LIBRARY_PATH="${llvm.stdenv.cc.cc.lib}/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
            export NIX_ENFORCE_NO_NATIVE=0
            export POLYCC="''${POLYCC:-$HOME/code/repo/pluto-dev/polycc}"
            export PLUTO="$POLYCC"
            export PATH="$PWD/build/_deps/barvinok/install/bin:$PATH"
          '';
        };
      }
    );
}
