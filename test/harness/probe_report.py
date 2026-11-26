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
from pathlib import Path

PIPELINES = ("default", "pluto-like", "algebraic", "hybrid", "tpz", "handtuned", "pluto")


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--probe-dir", required=True, type=Path)
    parser.add_argument("--ctest", default="ctest")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--report-only", action="store_true")
    return parser.parse_args()


def read_status(path):
    lines = path.read_text(errors="replace").splitlines()
    status = lines[0].split(" ", 1)[0] if lines else ""
    return status or "unknown"


def report(probe_dir):
    allowlist_file = probe_dir / "allowlist.txt"
    allowlist = []
    if allowlist_file.exists():
        allowlist = [line for line in allowlist_file.read_text().splitlines() if line]

    cases = probe_dir / "cases"
    results = sorted(p.relative_to(cases).as_posix() for p in cases.glob("*/*/*/result.txt"))
    cells = {}  # (row, pipeline) -> status
    rows = []
    csv = ["suite,kernel,pipeline,status,allowlisted"]
    candidates, regressions = [], []
    for result in results:
        suite, kernel, pipeline = result.split("/")[:3]
        status = read_status(cases / result)
        key = f"{suite}/{kernel}/{pipeline}"
        listed = key in allowlist
        if status == "pass":
            if not listed and pipeline != "pluto":  # reference runs, never allowlisted
                candidates.append(key)
        elif listed:
            regressions.append(f"{key} ({status})")
        csv.append(f"{suite},{kernel},{pipeline},{status},{'yes' if listed else 'no'}")
        row = f"{suite}/{kernel}"
        if row not in rows:
            rows.append(row)
        cells[(row, pipeline)] = f"**{status}**" if listed else status

    # Cases that never wrote a result were killed (ctest timeout).
    probed = {f"{row}/{pipeline}" for row, pipeline in cells}
    regressions += [f"{key} (not probed)" for key in allowlist if key not in probed]

    passing = sum(1 for result in results if read_status(cases / result) == "pass")
    md = [f"# Benchmark probe\n\n{passing}/{len(results)} cases pass. "
          "Bold cells are allowlisted in test/regression/benchmarks.cmake; "
          "`-` means the pipeline does not apply. Details: `cases/<suite>/<kernel>/<pipeline>/`.\n",
          "| kernel |" + "".join(f" {p} |" for p in PIPELINES),
          "|---|" + "---|" * len(PIPELINES)]
    md += [f"| {row} |" + "".join(f" {cells.get((row, p), '-')} |" for p in PIPELINES) for row in rows]
    md.append("\n## Passing but not allowlisted\n")
    md += [f"- {c}" for c in candidates] or ["None."]
    md.append("\n## Allowlisted but failing\n")
    md += [f"- {r}" for r in regressions] or ["None."]
    md = "\n".join(md) + "\n"

    (probe_dir / "probe-report.md").write_text(md)
    (probe_dir / "probe-report.csv").write_text("\n".join(csv) + "\n")
    print(md)
    print(f"Report: {probe_dir / 'probe-report.md'}")
    if regressions:
        print(f"{len(regressions)} allowlisted case(s) failed in the probe", file=sys.stderr)
        return 1
    return 0


def main():
    args = parse_args()
    probe_dir = args.probe_dir.resolve()
    if not args.report_only:
        shutil.rmtree(probe_dir / "cases", ignore_errors=True)
        subprocess.run([args.ctest, "--test-dir", probe_dir, "--parallel", str(args.jobs), "--progress",
                        "--output-log", probe_dir / "ctest.log"])
    sys.exit(report(probe_dir))


if __name__ == "__main__":
    main()
