"""Differential check of one synthetic fixture through one pipeline.

Each test owns its directory. A failure retains sources, commands, diagnostics,
and complete dumps; rerun exactly one case with ctest -R '^regression-...$'.
"""

import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness  # noqa: E402  # pylint: disable=wrong-import-position

# Problem sizes n, passed as argv[1] to every fixture program (fixture.h
# accepts 0..17). 0 gives an empty iteration domain; 1-4 give domains smaller
# than most tiles; 7, 8, 9 sit just below, at, and above a round size, so some
# runs leave partial tiles; 17 is the largest size fixture.h accepts.
SIZES = (0, 1, 2, 3, 4, 7, 8, 9, 17)
# Seeds, passed as argv[2]: they pick the initial array contents
# (fixture.h: init_matrix). Two fixed data patterns exercise values
# independently of domain shape.
SEEDS = (1, 7)
# OMP_NUM_THREADS for each run of the transformed program: 1 checks the
# sequential semantics, 2 and 4 expose races in the generated parallel loops.
THREADS = (1, 2, 4)
# Pipelines with parameter tiling: pesto leaves the tile sizes symbolic, and the
# generated code reads them from the DIV0, DIV1, DIV2 macros at compile time.
# A run of these pipelines that emits no DIV0 fails: the tiling never happened.
PARAM_TILED = ("hybrid", "algebraic")
# Compiler flags that set those tile sizes, one -DDIVn per tiled dimension.
# The transformed program is compiled and checked once per entry used:
#   standard   no macros: for pipelines without parameter tiling
#   unit       1x1x1 tiles: one iteration per tile, the degenerate case
#   uneven     3, 5, 7: distinct, non-dividing sizes, so tiles end partially
#              and a swapped dimension would show up
#   oversized  64, 65, 66: larger than any domain (n <= 17), so each
#              dimension fits in a single, partial tile
DIVIDERS: dict[str, list[str]] = {
    "standard": [],
    "unit": ["-DDIV0=1", "-DDIV1=1", "-DDIV2=1"],
    "uneven": ["-DDIV0=3", "-DDIV1=5", "-DDIV2=7"],
    "oversized": ["-DDIV0=64", "-DDIV1=65", "-DDIV2=66"],
}


class Args(harness.CommonArgs):  # pylint: disable=too-few-public-methods
    """Command-line arguments."""

    fixture: str
    pipeline: str
    tile: str | None


def parse_args() -> Args:
    """Parse the command line."""
    parser = harness.common_parser(__doc__)
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--pipeline", required=True)
    parser.add_argument("--tile", help="fills @TILE@ (fixed Pluto tile sizes); parameter-tiling "
                                       "pipelines take their tile sizes as DIVn macros at "
                                       "compile time")
    return parser.parse_args(namespace=Args())


def body(args: Args, case: harness.Case) -> None:
    """Transform the fixture, then compare the transformed program with the original."""
    fixtures = args.source_dir / "fixtures"
    shutil.copyfile(fixtures / f"{args.fixture}.c", case.path("input.c"))
    shutil.copyfile(fixtures / "fixture.h", case.path("fixture.h"))
    config_args = case.pipeline_args(args.source_dir / "pipelines", args.pipeline, args.tile)
    param_tiled = args.pipeline in PARAM_TILED
    cflags = harness.cflags(args.sanitizers)

    case.path("generated.c").unlink(missing_ok=True)
    # A previous successful run must not supply a header that this run forgot to emit.
    for header in case.dir.glob("generated.trahrhe.*.h"):
        header.unlink()
    case.run("transform", "transform-fail",
             [args.pesto, *config_args, "input.c", "-o", "generated.c"])
    if not case.path("generated.c").exists():
        case.fail("transform-fail", "no generated source")
    if param_tiled and "DIV0" not in case.path("generated.c").read_text(encoding="utf-8"):
        case.fail("transform-fail", "parameter tiling was not exercised")

    case.run("compile-baseline", "baseline-fail",
             [args.cc, *cflags, "input.c", "-o", "baseline", "-lm"])
    for n in SIZES:
        for seed in SEEDS:
            phase = f"baseline-n{n}-seed{seed}"
            case.run(phase, "baseline-fail", [case.path("baseline"), n, seed], threads=1)
            if case.path(f"{phase}.out").stat().st_size == 0:
                case.fail("baseline-fail", "baseline produced no result dump")

    dividers = ("unit", "uneven", "oversized") if param_tiled else ("standard",)
    for divider in dividers:
        case.run(f"compile-{divider}", "compile-fail",
                 [args.cc, *cflags, *DIVIDERS[divider], "generated.c", "-o", "transformed", "-lm"])
        for n in SIZES:
            for seed in SEEDS:
                instance = f"n{n}-seed{seed}"
                expected = harness.sha256_file(case.path(f"baseline-{instance}.out"))
                for threads in THREADS:
                    phase = f"{divider}-{instance}-t{threads}"
                    case.run(phase, "crash", [case.path("transformed"), n, seed],
                             threads=threads)
                    if harness.sha256_file(case.path(f"{phase}.out")) != expected:
                        case.fail("mismatch",
                                  f"{divider}, {instance}, threads={threads}\n"
                                  f"Expected: {case.path(f'baseline-{instance}.out')}\n"
                                  f"Actual: {case.path(f'{phase}.out')}")
    case.passed()


def main() -> None:
    """Entry point."""
    args = parse_args()
    harness.case_main(lambda: harness.Case(args.work_dir, f"{args.fixture}/{args.pipeline}"),
                      lambda case: body(args, case))


if __name__ == "__main__":
    main()
