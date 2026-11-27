"""Run the benchmark probe and summarise it.

    probe_report.py --probe-dir <build>/bench-probe [--ctest ctest] [--jobs n] [--report-only]

Without --report-only, runs every probe case (failures are expected and do not
stop the run). A subset can be rerun by hand with
    ctest --test-dir <build>/bench-probe -R <regex>
followed by this script with --report-only.

Writes probe-report.md and probe-report.csv into the probe directory and
compares the outcome with allowlist.txt (the pairs registered by pesto_bench()).
Exits 1 when an allowlisted pair fails.
"""

import argparse
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

PIPELINES = ("default", "pluto-like", "algebraic", "hybrid", "tpz", "handtuned", "pluto")


class Args(argparse.Namespace):  # pylint: disable=too-few-public-methods
    """Command-line arguments."""

    probe_dir: Path
    ctest: str
    jobs: int
    report_only: bool


def parse_args() -> Args:
    """Parse the command line."""
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--probe-dir", required=True, type=Path)
    parser.add_argument("--ctest", default="ctest")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--report-only", action="store_true")
    return parser.parse_args(namespace=Args())


def read_status(path: Path) -> str:
    """First word of a result.txt."""
    lines = path.read_text(errors="replace").splitlines()
    status = lines[0].split(" ", 1)[0] if lines else ""
    return status or "unknown"


@dataclass
class Probe:
    """The collected outcome of the probe cases."""

    total: int = 0
    passing: int = 0
    rows: list[str] = field(default_factory=list[str])  # suite/kernel, in order
    cells: dict[tuple[str, str], str] = field(default_factory=dict[tuple[str, str], str])
    csv: list[str] = field(default_factory=lambda: ["suite,kernel,pipeline,status,allowlisted"])
    candidates: list[str] = field(default_factory=list[str])  # passing, not allowlisted
    regressions: list[str] = field(default_factory=list[str])  # allowlisted, failing

    def add(self, key: str, status: str, listed: bool) -> None:
        """Record the status of one case (key: suite/kernel/pipeline)."""
        suite, kernel, pipeline = key.split("/")
        self.total += 1
        if status == "pass":
            self.passing += 1
            if not listed and pipeline != "pluto":  # reference runs, never allowlisted
                self.candidates.append(key)
        elif listed:
            self.regressions.append(f"{key} ({status})")
        self.csv.append(f"{suite},{kernel},{pipeline},{status},{'yes' if listed else 'no'}")
        row = f"{suite}/{kernel}"
        if row not in self.rows:
            self.rows.append(row)
        self.cells[(row, pipeline)] = f"**{status}**" if listed else status

    def markdown(self) -> str:
        """The Markdown report."""
        md = [f"# Benchmark probe\n\n{self.passing}/{self.total} cases pass. "
              "Bold cells are tested pipelines of test/regression/benchmarks.txt; "
              "`-` means the pipeline does not apply. "
              "Details: `cases/<suite>/<kernel>/<pipeline>/`.\n",
              "| kernel |" + "".join(f" {p} |" for p in PIPELINES),
              "|---|" + "---|" * len(PIPELINES)]
        md += [f"| {row} |" + "".join(f" {self.cells.get((row, p), '-')} |" for p in PIPELINES)
               for row in self.rows]
        md.append("\n## Passing but not allowlisted\n")
        md += [f"- {c}" for c in self.candidates] or ["None."]
        md.append("\n## Allowlisted but failing\n")
        md += [f"- {r}" for r in self.regressions] or ["None."]
        return "\n".join(md) + "\n"


def collect(probe_dir: Path) -> Probe:
    """Read the result of every case and compare it with the allowlist."""
    allowlist_file = probe_dir / "allowlist.txt"
    allowlist: list[str] = []
    if allowlist_file.exists():
        allowlist = [line for line in allowlist_file.read_text(encoding="utf-8").splitlines()
                     if line]

    cases = probe_dir / "cases"
    probe = Probe()
    for result in sorted(p.relative_to(cases).as_posix() for p in cases.glob("*/*/*/result.txt")):
        key = result.removesuffix("/result.txt")
        probe.add(key, read_status(cases / result), key in allowlist)

    # Cases that never wrote a result were killed (ctest timeout).
    probed = {f"{row}/{pipeline}" for row, pipeline in probe.cells}
    probe.regressions += [f"{key} (not probed)" for key in allowlist if key not in probed]
    return probe


def report(probe_dir: Path) -> int:
    """Write the reports; return the exit code (1 when an allowlisted case failed)."""
    probe = collect(probe_dir)
    md = probe.markdown()
    (probe_dir / "probe-report.md").write_text(md, encoding="utf-8")
    (probe_dir / "probe-report.csv").write_text("\n".join(probe.csv) + "\n", encoding="utf-8")
    print(md)
    print(f"Report: {probe_dir / 'probe-report.md'}")
    if probe.regressions:
        print(f"{len(probe.regressions)} allowlisted case(s) failed in the probe", file=sys.stderr)
        return 1
    return 0


def main() -> None:
    """Entry point."""
    args = parse_args()
    probe_dir = args.probe_dir.resolve()
    if not args.report_only:
        shutil.rmtree(probe_dir / "cases", ignore_errors=True)
        # Failing cases are expected: the report classifies them.
        subprocess.run([args.ctest, "--test-dir", probe_dir, "--parallel", str(args.jobs),
                        "--progress", "--output-log", probe_dir / "ctest.log"], check=False)
    sys.exit(report(probe_dir))


if __name__ == "__main__":
    main()
