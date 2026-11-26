"""Differential check of one PolyBench or NPDP kernel through one pipeline.

The original and the transformed kernel are compiled with the same flags and
run on several problem sizes; the transformed one with several tile divider
sets and thread counts. Result dumps (stderr) must hash identically.

--pipeline is a pipelines/*.json.in name, "default", "handtuned" (with
--handtuned-config), or "pluto" (with --polycc [--polycc-flags]).
"""

import itertools
import re
import shlex
import shutil
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness  # noqa: E402

THREADS = (1, 2, 4)
PARAM_DIVIDERS = [
    ("unit", ["-DDIV0=1", "-DDIV1=1", "-DDIV2=1", "-DDIV3=1"]),
    ("uneven", ["-DDIV0=3", "-DDIV1=5", "-DDIV2=7", "-DDIV3=11"]),
    ("oversized", ["-DDIV0=64", "-DDIV1=65", "-DDIV2=66", "-DDIV3=67"]),
]
# TpzTiling: DIV0 is the band (gap) width, DIV2 the inner tile size; DIV1 keeps
# its generated default. Ranges follow the pesto-bench TPZ runs (DIV0 >= 2,
# DIV2 >= 4).
TPZ_DIVIDERS = [
    ("small", ["-DDIV0=2", "-DDIV2=4"]),
    ("uneven", ["-DDIV0=3", "-DDIV2=5"]),
    ("oversized", ["-DDIV0=64", "-DDIV2=67"]),
]


def parse_args():
    parser = harness.common_parser(__doc__)
    parser.add_argument("--suite", required=True, choices=("polybench", "npdp"))
    parser.add_argument("--kernel", required=True, help="path inside --bench-root")
    parser.add_argument("--bench-root", required=True, type=Path)
    parser.add_argument("--pipeline", required=True)
    parser.add_argument("--handtuned-config", type=Path)
    parser.add_argument("--polycc", default="")
    parser.add_argument("--polycc-flags", default="")
    parser.add_argument("--python", default=sys.executable)
    parser.add_argument("--gen-maxcut", type=Path)
    parser.add_argument("--iscc")
    return parser.parse_args()


@dataclass
class Maxcut:
    """The gen_maxcut.py command that TpzTiling documents in its output."""
    domain: str
    stride: int = 1
    f1: list = field(default_factory=list)
    params: list = field(default_factory=list)

    @classmethod
    def parse(cls, case, generated):
        match = re.search(r'--domain "([^"]*)"', generated)
        if not match:
            case.fail("maxcut-fail", "no gen_maxcut domain in the generated source")
        maxcut = cls(match.group(1))
        if match := re.search(r"--t \$\(\(DIV0 \* ([0-9]+)\)\)", generated):
            maxcut.stride = int(match.group(1))
        if match := re.search(r"--f1 (-?[0-9]+)", generated):
            maxcut.f1 = ["--f1", match.group(1)]
        # [p1, p2] -> { ... }
        arrow = maxcut.domain.find("->")
        if maxcut.domain.startswith("[") and arrow > 0:
            head = re.sub(r"^\[(.*)\] *$", r"\1", maxcut.domain[:arrow])
            maxcut.params = [p.strip() for p in head.split(",") if p.strip()]
        return maxcut


class Bench:
    def __init__(self, args, case):
        self.args, self.case = args, case
        self.name = Path(args.kernel).name
        self.cflags = harness.cflags(args.sanitizers)
        root, work = args.bench_root, case.dir
        if args.suite == "polybench":
            self.variants = [("mini", ["-DMINI_DATASET"]), ("small", ["-DSMALL_DATASET"])]
            self.suite_flags = ["-I", root / "utilities", "-I", work, "-DPOLYBENCH_DUMP_ARRAYS"]
            self.suite_sources = [root / "utilities/polybench.c"]
        else:
            # The transformed code assumes the parameters are at least 10
            # (pesto's default context), so smaller sizes are out of contract.
            self.variants = [(f"n{n}-s{seed}", [f"-DN={n}", f"-DNPDP_RSEED={seed}"])
                             for n in (10, 23, 64) for seed in (1, 7)]
            self.suite_flags = ["-I", root / "include", "-I", work, "-DNPDP_DUMP"]
            self.suite_sources = []

    def prepare(self):
        """Fresh copy of the kernel sources: generated headers from an earlier
        run must not hide one that this run forgot to emit."""
        work = self.case.dir
        for pattern in ("*.c", "*.h", "*.err", "*.out"):
            for stale in work.glob(pattern):
                stale.unlink()
        source = self.args.bench_root / self.args.kernel
        for path in sorted(source.glob("*.[ch]")):
            shutil.copy(path, work)

    def transform(self):
        args, case = self.args, self.case
        if args.pipeline == "pluto":
            # Reference run: the same kernel through Pluto's polycc, to tell
            # pesto bugs from kernels that the polyhedral toolchain does not
            # handle. --nounroll (as in pesto-bench): unrolled output takes
            # minutes to build with sanitizers.
            if not args.polycc:
                case.fail("transform-fail", "pluto pipeline needs --polycc")
            case.run("transform", "transform-fail",
                     [*shlex.split(args.polycc), f"{self.name}.c", "--tile", "--parallel", "--nounroll",
                      *shlex.split(args.polycc_flags), "-o", "generated.c"])
        else:
            if args.pipeline == "handtuned":
                shutil.copyfile(args.handtuned_config, case.path("pipeline.json"))
                config_args = ["--config", case.path("pipeline.json")]
            else:
                # 7 fills @TILE@ (pluto-like); the parameter-tiling pipelines
                # have none: their tile sizes are the DIVn dividers below.
                config_args = case.pipeline_args(args.source_dir / "pipelines", args.pipeline, 7)
            case.run("transform", "transform-fail",
                     [args.pesto, *config_args, f"{self.name}.c", "-o", "generated.c"])
        if not case.path("generated.c").exists():
            case.fail("transform-fail", "no generated source")
        return case.path("generated.c").read_text()

    def param_values(self, maxcut, tag, defines):
        """Values of the gen_maxcut domain parameters for one problem instance."""
        case = self.case
        if self.args.suite == "npdp":
            values = []
            for param in maxcut.params:
                if param == "n":
                    values.append(re.search(r"-DN=([0-9]+)", " ".join(defines)).group(1))
                elif param == "l":
                    values.append("0")  # NPDP_MIN_LOOP default
                else:
                    case.fail("maxcut-fail", f"unknown NPDP parameter '{param}'")
            return values
        # _PB_X expands to the scalar x (POLYBENCH_LOOP_BOUND); its value is
        # the dataset macro X.
        prints = "".join(f'  printf("%ld;", (long)({re.sub(r"^_PB_", "", p)}));\n' for p in maxcut.params)
        case.path(f"params-{tag}.c").write_text(
            f'#include <stdio.h>\n#include "polybench.h"\n#include "{self.name}.h"\n'
            f"int main(void) {{\n{prints}  return 0;\n}}\n")
        case.run(f"params-{tag}-build", "maxcut-fail",
                 [self.args.cc, *self.suite_flags, *defines, f"params-{tag}.c", "-o", f"params-{tag}"])
        case.run(f"params-{tag}", "maxcut-fail", [case.path(f"params-{tag}")])
        return case.path(f"params-{tag}.out").read_text().rstrip(";").split(";")

    def write_maxcut(self, maxcut, tag, defines, div0):
        """maxcut.h for this problem instance. gen_maxcut.py keeps the domain
        parameters as arguments, while the generated code calls
        max_cut_volume(b): bind them to this instance's values."""
        values = self.param_values(maxcut, tag, defines) if maxcut.params else []
        # Pinning the parameters to this instance also keeps lexmin(x1) from
        # being piecewise in a parameter (e.g. NPDP l), which gen_maxcut
        # cannot use.
        domain = maxcut.domain
        pins = "".join(f"{p} = {v} and "
                       for p, v in itertools.zip_longest(maxcut.params, values, fillvalue=""))
        if pins:
            domain = re.sub(r"^([^{]*\{[^:]*:)", lambda m: f"{m.group(1)} {pins}", domain, count=1)
        t = div0 * maxcut.stride
        self.case.run(f"maxcut-{tag}-t{t}", "maxcut-fail",
                      [self.args.python, self.args.gen_maxcut, "--domain", domain, "--t", t, *maxcut.f1,
                       "--func", "pesto_maxcut_impl", "--iscc", self.args.iscc, "--no-backup",
                       "--out", "maxcut.h"])
        bound = "".join(f"{v}, " for v in values)
        with open(self.case.path("maxcut.h"), "a") as header:
            header.write(f"\nstatic long max_cut_volume(long b) {{ return pesto_maxcut_impl({bound}b); }}\n")

    def __call__(self):
        args, case = self.args, self.case
        self.prepare()
        generated = self.transform()
        param_tiled = "DIV0" in generated
        # TpzTiling leaves the max_cut_volume() helper to gen_maxcut.py.
        maxcut = Maxcut.parse(case, generated) if '#include "maxcut.h"' in generated else None

        dividers = [("standard", [])]
        if param_tiled:
            dividers = PARAM_DIVIDERS
        if maxcut:
            dividers = TPZ_DIVIDERS
        compile_cmd = [args.cc, *self.cflags, *self.suite_flags]

        for tag, defines in self.variants:
            case.run(f"compile-baseline-{tag}", "baseline-fail",
                     [*compile_cmd, *defines, *self.suite_sources, f"{self.name}.c", "-o", f"baseline-{tag}", "-lm"])
            case.run(f"baseline-{tag}", "baseline-fail", [case.path(f"baseline-{tag}")], threads=1)
            if case.path(f"baseline-{tag}.err").stat().st_size == 0:
                case.fail("baseline-fail", f"baseline produced no result dump ({tag})")
            expected = harness.dump_hash(case.path(f"baseline-{tag}.err"))

            for dtag, tile_flags in dividers:
                run = f"{tag}-{dtag}"
                if maxcut:
                    div0 = 1
                    if match := re.search(r"-DDIV0=([0-9]+)", " ".join(tile_flags)):
                        div0 = int(match.group(1))
                    elif match := re.search(r"#define DIV0 ([0-9]+)", generated):
                        div0 = int(match.group(1))
                    self.write_maxcut(maxcut, tag, defines, div0)
                # maxcut.h can reach megabytes for large DIV0: allow a slow build.
                case.run(f"compile-{run}", "compile-fail",
                         [*compile_cmd, *defines, *tile_flags, *self.suite_sources, "generated.c",
                          "-o", f"transformed-{run}", "-lm"], timeout=300)
                for threads in THREADS:
                    phase = f"{run}-t{threads}"
                    case.run(phase, "crash", [case.path(f"transformed-{run}")], threads=threads)
                    if harness.dump_hash(case.path(f"{phase}.err")) != expected:
                        case.fail("mismatch", f"{run}, threads={threads}\n"
                                              f"Expected: {case.path(f'baseline-{tag}.err')}\n"
                                              f"Actual: {case.path(f'{phase}.err')}")
                case.path(f"transformed-{run}").unlink()

        if args.pipeline in ("algebraic", "hybrid") and not param_tiled:
            case.passed("pass-untiled")
        else:
            case.passed()


if __name__ == "__main__":
    args = parse_args()
    harness.case_main(lambda: harness.Case(args.work_dir, f"{args.suite}/{args.kernel}/{args.pipeline}"),
                      lambda case: Bench(args, case)())
