"""Differential check of one PolyBench or NPDP kernel through one pipeline.

The original and the transformed kernel are compiled with the same flags and
run on several problem sizes; the transformed one with several tile divider
sets and thread counts. Result dumps (stderr) must hash identically.

--pipeline is a pipelines/*.json.in name, "default", "handtuned" (with
--handtuned-config), or "pluto" (with --polycc [--polycc-flags]).
"""

import ast
import operator
import re
import shlex
import shutil
import sys
from collections.abc import Callable
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
# pylint: disable=wrong-import-position
import harness  # noqa: E402
from harness import Case, CmdArg  # noqa: E402

# pylint: enable=wrong-import-position

# (tag, compiler flags): a problem instance, or a set of tile dividers.
Variant = tuple[str, list[str]]

THREADS = (1, 2, 4)
PARAM_DIVIDERS: list[Variant] = [
    ("uneven", ["-DDIV0=3", "-DDIV1=5", "-DDIV2=7", "-DDIV3=11"]),
    ("oversized", ["-DDIV0=64", "-DDIV1=65", "-DDIV2=66", "-DDIV3=67"]),
]
# TpzTiling: DIV0 is the band (gap) width, DIV2 the inner tile size; DIV1 keeps
# its generated default. Ranges follow the pesto-bench TPZ runs (DIV0 >= 2,
# DIV2 >= 4).
TPZ_DIVIDERS: list[Variant] = [
    ("small", ["-DDIV0=2", "-DDIV2=4"]),
    ("uneven", ["-DDIV0=3", "-DDIV2=5"]),
    ("oversized", ["-DDIV0=64", "-DDIV2=67"]),
]

POLYBENCH_VARIANTS: list[Variant] = [
    ("medium", ["-DMEDIUM_DATASET"]),
]

NPDP_VARIANTS = [
    (f"n{n}-s{seed}", [f"-DN={n}", f"-DNPDP_RSEED={seed}"])
    for n in (500,)
    for seed in (1, 7)
]


class Args(harness.CommonArgs):  # pylint: disable=too-few-public-methods
    """Command-line arguments."""

    suite: str
    kernel: str
    bench_root: Path
    pipeline: str
    handtuned_config: Path | None
    polycc: str
    polycc_flags: str
    python: str
    gen_maxcut: Path | None
    iscc: str | None


def parse_args() -> Args:
    """Parse the command line."""
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
    return parser.parse_args(namespace=Args())


@dataclass
class Maxcut:
    """The gen_maxcut.py command that TpzTiling documents in its output."""

    domain: str
    stride: int = 1
    f1: list[str] = field(default_factory=list[str])
    params: list[str] = field(default_factory=list[str])

    @classmethod
    def parse(cls, case: Case, generated: str) -> "Maxcut":
        """Extract the command from the generated source."""
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

    def instantiate(self, values: list[str]) -> str:
        """The domain with its parameters replaced by values: a concrete
        polytope '[] -> { [vars] : body }'. The constraints left without any
        variable (pesto's parameter context, e.g. '_PB_N >= 10') are checked,
        then dropped. Raise ValueError when one does not hold."""
        match = re.fullmatch(
            r"\s*\[[^\]]*\]\s*->\s*\{\s*\[([^\]]*)\]\s*:(.*)\}\s*", self.domain, re.S
        )
        if not match:
            raise ValueError(f"unexpected domain syntax: {self.domain}")
        variables, body = match.group(1).strip(), match.group(2)
        for param, value in zip(self.params, values, strict=True):
            # isl writes products as '4_PB_N': keep the coefficient.
            body = re.sub(
                rf"(?<!\w)(\d+)?{re.escape(param)}(?!\w)",
                lambda m, v=value: f"{m.group(1)}*{v}" if m.group(1) else v,
                body,
            )
        kept: list[str] = []
        conjuncts = _split_top_level(body, "and")
        if len(_split_top_level(body, "or")) > 1:
            conjuncts = [body.strip()]  # a disjunction: substitute only
        for conjunct in conjuncts:
            if re.search(r"[A-Za-z_]", conjunct):
                kept.append(conjunct)
            elif not _holds(conjunct):
                raise ValueError(f"instance violates the domain context: {conjunct}")
        return f"[] -> {{ [{variables}] : {' and '.join(kept)} }}"


def _split_top_level(text: str, keyword: str) -> list[str]:
    """Split text on ' keyword ' outside parentheses."""
    parts: list[str] = []
    depth, start = 0, 0
    for match in re.finditer(rf"[()]|\s{keyword}\s", text):
        token = match.group()
        if token == "(":
            depth += 1
        elif token == ")":
            depth -= 1
        elif depth == 0:
            end = match.start()
            parts.append(text[start:end].strip())
            start = match.end()
    parts.append(text[start:].strip())
    return [p for p in parts if p]


_COMPARE: dict[type[ast.cmpop], Callable[[int, int], bool]] = {
    ast.Lt: operator.lt,
    ast.LtE: operator.le,
    ast.Gt: operator.gt,
    ast.GtE: operator.ge,
    ast.Eq: operator.eq,
}
_ARITH: dict[type[ast.operator], Callable[[int, int], int]] = {
    ast.Add: operator.add,
    ast.Sub: operator.sub,
    ast.Mult: operator.mul,
}


def _holds(constraint: str) -> bool:
    """Evaluate a variable-free isl constraint such as '100 >= 10' or
    '0 <= 2*3 - 1 < 9'. One it cannot read is assumed to hold (isl will)."""
    python = re.sub(r"(?<![<>=!])=(?!=)", "==", constraint)
    try:
        tree = ast.parse(python, mode="eval").body
    except SyntaxError:
        return True

    def value(node: ast.expr) -> int:
        if isinstance(node, ast.Constant) and isinstance(node.value, int):
            return node.value
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
            return -value(node.operand)
        if isinstance(node, ast.BinOp) and type(node.op) in _ARITH:
            return _ARITH[type(node.op)](value(node.left), value(node.right))
        raise TypeError(ast.dump(node))

    if not isinstance(tree, ast.Compare):
        return True
    try:
        left = value(tree.left)
        for op, right_node in zip(tree.ops, tree.comparators):
            if type(op) not in _COMPARE:
                return True
            right = value(right_node)
            if not _COMPARE[type(op)](left, right):
                return False
            left = right
    except TypeError:
        return True
    return True


def tpz_div0(tile_flags: list[str], generated: str) -> int:
    """The band width DIV0: set by the tile flags, or defaulted in the generated code."""
    if match := re.search(r"-DDIV0=([0-9]+)", " ".join(tile_flags)):
        return int(match.group(1))
    if match := re.search(r"#define DIV0 ([0-9]+)", generated):
        return int(match.group(1))
    return 1


class Bench:
    """One kernel through one pipeline, in the work directory of its Case."""

    def __init__(self, args: Args, case: Case) -> None:
        self.args, self.case = args, case
        self.name = Path(args.kernel).name
        self.cflags = harness.cflags(args.sanitizers)
        root, work = args.bench_root, case.dir
        self.variants: list[Variant]
        self.suite_flags: list[CmdArg]
        self.suite_sources: list[Path]
        if args.suite == "polybench":
            self.variants = POLYBENCH_VARIANTS
            self.suite_flags = [
                "-I",
                root / "utilities",
                "-I",
                work,
                "-DPOLYBENCH_DUMP_ARRAYS",
            ]
            self.suite_sources = [root / "utilities/polybench.c"]
        else:
            # The transformed code assumes the parameters are at least 10
            # (pesto's default context), so smaller sizes are out of contract.
            self.variants = NPDP_VARIANTS
            self.suite_flags = ["-I", root / "include", "-I", work, "-DNPDP_DUMP"]
            self.suite_sources = []

    def prepare(self) -> None:
        """Fresh copy of the kernel sources: generated headers from an earlier
        run must not hide one that this run forgot to emit."""
        work = self.case.dir
        for pattern in ("*.c", "*.h", "*.err", "*.out"):
            for stale in work.glob(pattern):
                stale.unlink()
        source = self.args.bench_root / self.args.kernel
        for path in sorted(source.glob("*.[ch]")):
            shutil.copy(path, work)

    def transform(self) -> str:
        """Transform the kernel into generated.c; return its source."""
        args, case = self.args, self.case
        if args.pipeline == "pluto":
            # Reference run: the same kernel through Pluto's polycc, to tell
            # pesto bugs from kernels that the polyhedral toolchain does not
            # handle. --nounroll (as in pesto-bench): unrolled output takes
            # minutes to build with sanitizers.
            if not args.polycc:
                case.fail("transform-fail", "pluto pipeline needs --polycc")
            case.run(
                "transform",
                "transform-fail",
                [
                    *shlex.split(args.polycc),
                    f"{self.name}.c",
                    "--tile",
                    "--parallel",
                    "--nounroll",
                    *shlex.split(args.polycc_flags),
                    "-o",
                    "generated.c",
                ],
            )
        else:
            config_args: list[CmdArg]
            if args.pipeline == "handtuned":
                if args.handtuned_config is None:
                    case.fail(
                        "transform-fail", "handtuned pipeline needs --handtuned-config"
                    )
                shutil.copyfile(args.handtuned_config, case.path("pipeline.json"))
                config_args = ["--config", case.path("pipeline.json")]
            else:
                # 7 fills @TILE@ (pluto-like); the parameter-tiling pipelines
                # have none: their tile sizes are the DIVn dividers below.
                config_args = [
                    *case.pipeline_args(args.source_dir / "pipelines", args.pipeline, 7)
                ]
            case.run(
                "transform",
                "transform-fail",
                [args.pesto, *config_args, f"{self.name}.c", "-o", "generated.c"],
            )
        if not case.path("generated.c").exists():
            case.fail("transform-fail", "no generated source")
        return case.path("generated.c").read_text(encoding="utf-8")

    def param_values(self, maxcut: Maxcut, tag: str, defines: list[str]) -> list[str]:
        """Values of the gen_maxcut domain parameters for one problem instance."""
        case = self.case
        if self.args.suite == "npdp":
            values: list[str] = []
            for param in maxcut.params:
                if param == "n":
                    size = re.search(r"-DN=([0-9]+)", " ".join(defines))
                    if not size:
                        case.fail("maxcut-fail", f"no -DN in the defines of {tag}")
                    values.append(size.group(1))
                elif param == "l":
                    values.append("0")  # NPDP_MIN_LOOP default
                else:
                    case.fail("maxcut-fail", f"unknown NPDP parameter '{param}'")
            return values
        # _PB_X expands to the scalar x (POLYBENCH_LOOP_BOUND); its value is
        # the dataset macro X.
        prints = "".join(
            f'  printf("%ld;", (long)({re.sub(r"^_PB_", "", p)}));\n'
            for p in maxcut.params
        )
        case.path(f"params-{tag}.c").write_text(
            f'#include <stdio.h>\n#include "polybench.h"\n#include "{self.name}.h"\n'
            f"int main(void) {{\n{prints}  return 0;\n}}\n",
            encoding="utf-8",
        )
        case.run(
            f"params-{tag}-build",
            "maxcut-fail",
            [
                self.args.cc,
                *self.suite_flags,
                *defines,
                f"params-{tag}.c",
                "-o",
                f"params-{tag}",
            ],
        )
        case.run(f"params-{tag}", "maxcut-fail", [case.path(f"params-{tag}")])
        return (
            case.path(f"params-{tag}.out")
            .read_text(encoding="utf-8")
            .rstrip(";")
            .split(";")
        )

    def write_maxcut(
        self, maxcut: Maxcut, tag: str, defines: list[str], div0: int
    ) -> None:
        """maxcut.h for this problem instance and DIV0. The domain is
        instantiated with the instance's parameter values, so gen_maxcut.py
        emits max_cut_volume(b), exactly what the generated code calls. A
        concrete domain also keeps lexmin(x1) from being piecewise in a
        parameter (e.g. NPDP l), which gen_maxcut cannot use."""
        args, case = self.args, self.case
        if args.gen_maxcut is None or args.iscc is None:
            case.fail("maxcut-fail", "TpzTiling output needs --gen-maxcut and --iscc")
        values = self.param_values(maxcut, tag, defines) if maxcut.params else []
        try:
            domain = maxcut.instantiate(values)
        except ValueError as error:
            case.fail("maxcut-fail", str(error))
        t = div0 * maxcut.stride
        case.run(
            f"maxcut-{tag}-t{t}",
            "maxcut-fail",
            [
                args.python,
                args.gen_maxcut,
                "--domain",
                domain,
                "--t",
                t,
                *maxcut.f1,
                "--func",
                "max_cut_volume",
                "--iscc",
                args.iscc,
                "--no-backup",
                "--out",
                "maxcut.h",
            ],
        )

    def check_variant(
        self,
        variant: Variant,
        dividers: list[Variant],
        generated: str,
        maxcut: Maxcut | None,
    ) -> None:
        """Compare the transformed kernel with the original on one problem instance."""
        case = self.case
        tag, defines = variant
        compile_cmd: list[CmdArg] = [
            self.args.cc,
            *self.cflags,
            *self.suite_flags,
            *defines,
        ]
        case.run(
            f"compile-baseline-{tag}",
            "baseline-fail",
            [
                *compile_cmd,
                *self.suite_sources,
                f"{self.name}.c",
                "-o",
                f"baseline-{tag}",
                "-lm",
            ],
        )
        case.run(
            f"baseline-{tag}",
            "baseline-fail",
            [case.path(f"baseline-{tag}")],
            threads=1,
        )
        if case.path(f"baseline-{tag}.err").stat().st_size == 0:
            case.fail("baseline-fail", f"baseline produced no result dump ({tag})")
        expected = harness.dump_hash(case.path(f"baseline-{tag}.err"))

        for dtag, tile_flags in dividers:
            run = f"{tag}-{dtag}"
            if maxcut:
                self.write_maxcut(maxcut, tag, defines, tpz_div0(tile_flags, generated))
            # maxcut.h can reach megabytes for large DIV0: allow a slow build.
            case.run(
                f"compile-{run}",
                "compile-fail",
                [
                    *compile_cmd,
                    *tile_flags,
                    *self.suite_sources,
                    "generated.c",
                    "-o",
                    f"transformed-{run}",
                    "-lm",
                ],
                timeout=300,
            )
            for threads in THREADS:
                phase = f"{run}-t{threads}"
                case.run(
                    phase, "crash", [case.path(f"transformed-{run}")], threads=threads
                )
                if harness.dump_hash(case.path(f"{phase}.err")) != expected:
                    case.fail(
                        "mismatch",
                        f"{run}, threads={threads}\n"
                        f"Expected: {case.path(f'baseline-{tag}.err')}\n"
                        f"Actual: {case.path(f'{phase}.err')}",
                    )
            case.path(f"transformed-{run}").unlink()

    def __call__(self) -> None:
        self.prepare()
        generated = self.transform()
        param_tiled = "DIV0" in generated
        # TpzTiling leaves the max_cut_volume() helper to gen_maxcut.py.
        maxcut = None
        if '#include "maxcut.h"' in generated:
            maxcut = Maxcut.parse(self.case, generated)

        dividers: list[Variant] = [("standard", [])]
        if param_tiled:
            dividers = PARAM_DIVIDERS
        if maxcut:
            dividers = TPZ_DIVIDERS
        for variant in self.variants:
            self.check_variant(variant, dividers, generated, maxcut)

        if self.args.pipeline in ("algebraic", "hybrid") and not param_tiled:
            self.case.passed("pass-untiled")
        else:
            self.case.passed()


def main() -> None:
    """Entry point."""
    args = parse_args()
    harness.case_main(
        lambda: Case(args.work_dir, f"{args.suite}/{args.kernel}/{args.pipeline}"),
        lambda case: Bench(args, case)(),
    )


if __name__ == "__main__":
    main()
