"""Inputs pesto must reject with a diagnostic, exit code 1, and no output."""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness  # noqa: E402


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pesto", required=True)
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    return parser.parse_args()


def body(args):
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=True)
    os.environ.update(harness.SANITIZER_ENV, TMPDIR=str(work))

    def reject(name, diagnostic, *pesto_args):
        generated = work / "generated.c"
        generated.unlink(missing_ok=True)
        out, err = work / f"{name}.out", work / f"{name}.err"
        with open(out, "wb") as out_file, open(err, "wb") as err_file:
            code = harness.spawn([args.pesto, *pesto_args, "-o", "generated.c"], work, 15,
                                 out_file, err_file)
        errors = err.read_text(errors="replace")
        if (code != 1 or not re.search(diagnostic, errors)
                or harness.SANITIZER_REPORT.search(errors) or generated.exists()):
            raise harness.CaseFailed(
                f"{name}: expected diagnostic '{diagnostic}' and exit 1, "
                f"got '{harness.describe_status(code)}'\n{errors}")

    fixtures = args.source_dir / "fixtures"
    shutil.copyfile(fixtures / "pointwise.c", work / "input.c")
    for tiles in ([0], [-3, 2], []):
        config = {"passes": [{"uuid": "pluto", "options": {"tile": True, "tile-sizes": tiles}},
                             {"uuid": "codegen-cloog"}]}
        (work / "invalid-tiles.json").write_text(json.dumps(config))
        reject(f"tiles-{json.dumps(tiles, separators=(',', ':'))}", "Tile sizes must be",
               "--config", "invalid-tiles.json", "input.c")

    # CLooG supports these domains (a positive regression exercises that path),
    # while Pluto's importer cannot represent their existential local dimensions.
    shutil.copyfile(fixtures / "guarded_stride.c", work / "stride.c")
    reject("stride", "Pluto does not support local dimensions", "stride.c")

    source = (work / "input.c").read_text().replace(
        "#pragma endscop",
        "#pragma endscop\n#pragma scop\nfor(i=0;i<n;i++) output[i][0] += 1;\n#pragma endscop")
    (work / "multiple.c").write_text(source)
    reject("multiple-regions", "Only one scop region", "multiple.c")

    leftovers = sorted(work.glob(".pesto-*"))
    if leftovers:
        raise harness.CaseFailed("Temporary workspace leak: " + " ".join(map(str, leftovers)))


if __name__ == "__main__":
    args = parse_args()
    harness.main(lambda: body(args))
