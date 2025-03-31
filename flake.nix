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
              #misc
              gdb
              valgrind

              shfmt
              bc
              cmake-format
              likwid

              gitlab-ci-local
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
