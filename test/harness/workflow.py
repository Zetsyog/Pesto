"""Exercise the public CLI, the generated C compiler, and the resulting program."""

import json
import os
import shutil
import stat
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness  # noqa: E402
from harness import CaseFailed  # noqa: E402

CASES = ("default", "json", "skew", "errors", "concurrent")


def parse_args():
    parser = harness.common_parser(__doc__)
    parser.add_argument("--case", required=True, choices=CASES)
    return parser.parse_args()


class Workflow:
    def __init__(self, args):
        self.args = args
        self.dir = args.work_dir.resolve()
        self.dir.mkdir(parents=True, exist_ok=True)
        os.environ.update(harness.SANITIZER_ENV, TMPDIR=str(self.dir))

    def _exec(self, cmd, timeout=30, env=None):
        out, err = self.dir / "last.out", self.dir / "last.err"
        with open(out, "wb") as out_file, open(err, "wb") as err_file:
            code = harness.spawn(cmd, self.dir, timeout, out_file, err_file, env)
        return code, out.read_text(errors="replace"), err.read_text(errors="replace")

    def run(self, *cmd):
        code, out, err = self._exec(cmd)
        if code != 0:
            raise CaseFailed(f"Command failed ({harness.describe_status(code)}): {list(map(str, cmd))}\n{out}\n{err}")
        return out

    def reject(self, *pesto_args):
        code, out, err = self._exec([self.args.pesto, *pesto_args])
        # Signals and timeouts must fail the test, rather than count as rejection.
        if code != 1:
            raise CaseFailed(f"Expected exit 1, got '{harness.describe_status(code)}': {list(pesto_args)}\n{out}\n{err}")

    def check_cleanup(self):
        leftovers = sorted(self.dir.glob(".pesto-*"))
        if leftovers:
            raise CaseFailed("Temporary directories were not removed: " + " ".join(map(str, leftovers)))

    def write_json(self, name, value):
        (self.dir / name).write_text(json.dumps(value))

    def errors(self, generated):
        work = self.dir
        self.run(self.args.pesto, "--help")
        self.run(self.args.pesto, "--version")
        self.reject()
        self.reject("--unknown", "input.c")
        self.reject("-o")
        self.reject("input.c", "extra.c")
        self.reject("missing.c")
        self.reject("input.c", "-o", "missing-directory/output.c")
        self.reject("input.c", "-o", "input.c")
        (work / "destination-directory").mkdir(exist_ok=True)
        (work / "destination-directory/sentinel").write_text("keep this directory")
        self.reject("input.c", "-o", "destination-directory")
        if not (work / "destination-directory/sentinel").exists():
            raise CaseFailed("Failed publication changed the destination directory")
        if (work / "input.c").read_bytes() != (self.args.source_dir / "kernel.c").read_bytes():
            raise CaseFailed("Failed invocation changed its input")

        (work / "bad.json").write_text("{broken")
        self.reject("--config", "bad.json", "input.c", "-o", generated)
        for config in ({"passes": [{}]}, {"passes": [{"uuid": "unknown"}]}, {"passes": {}},
                       # An incompatible pass before code generation must be rejected, too.
                       {"passes": [{"uuid": "codegen-clast"}, {"uuid": "pluto"}]}):
            self.write_json("bad.json", config)
            self.reject("--config", "bad.json", "input.c", "-o", generated)

        (work / "sentinel").write_text("keep this output")
        self.write_json("bad.json", {"passes": [{}]})
        self.reject("--config", "bad.json", "input.c", "-o", "sentinel")
        # Fail the external preprocessor after code generation has succeeded.
        fake_bin = work / "fake-bin"
        fake_bin.mkdir(exist_ok=True)
        fake_gcc = fake_bin / "gcc"
        fake_gcc.write_text("#!/bin/sh\nexit 42\n")
        fake_gcc.chmod(stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)
        saved_path = os.environ["PATH"]
        os.environ["PATH"] = f"{fake_bin}:{saved_path}"
        try:
            self.reject("input.c", "-o", "sentinel")
        finally:
            os.environ["PATH"] = saved_path
        if (work / "sentinel").read_text() != "keep this output":
            raise CaseFailed("Failed invocation changed existing output")

    def transform_and_compare(self, case, generated, output_args):
        args, work = self.args, self.dir
        config_args = []
        if case == "json":
            config_args = ["--config", args.source_dir / "default.json"]
        elif case == "skew":
            self.write_json("skew.json", {"passes": [{"uuid": "skew", "transform": ["x = i", "y = j"]},
                                                     {"uuid": "codegen-cloog"}]})
            config_args = ["--config", work / "skew.json"]
        self.run(args.pesto, *config_args, "input.c", *output_args)
        if not generated.exists():
            raise CaseFailed("PESTO reported success without writing output")

        for size in (1, 7, 24):
            self.run(args.cc, "-O2", "-fopenmp", f"-DSIZE={size}", "input.c", "-o", "baseline", "-lm")
            self.run(args.cc, "-O2", "-fopenmp", f"-DSIZE={size}", generated, "-o", "transformed", "-lm")
            code, expected, _ = self._exec([work / "baseline"], timeout=10)
            if code != 0:
                raise CaseFailed(f"Baseline failed: {harness.describe_status(code)}")
            for threads in (1, 2):
                env = dict(os.environ, OMP_NUM_THREADS=str(threads))
                code, actual, errors = self._exec([work / "transformed"], timeout=10, env=env)
                if code != 0 or actual != expected:
                    raise CaseFailed(f"Generated output differs: SIZE={size}, threads={threads}, "
                                     f"status={harness.describe_status(code)}\n{errors}")

    def __call__(self):
        args, work = self.args, self.dir
        shutil.copyfile(args.source_dir / "kernel.c", work / "input.c")
        (work / "output with spaces").mkdir(exist_ok=True)
        generated = work / "output with spaces/generated.c"
        output_args = ["-o", generated]
        if args.case == "default":
            generated = work / "input.pesto.c"
            output_args = []
        generated.unlink(missing_ok=True)

        if args.case == "errors":
            self.errors(generated)
        elif args.case == "concurrent":
            self.run("bash", args.source_dir / "concurrent.sh", args.pesto, args.cc)
        else:
            self.transform_and_compare(args.case, generated, output_args)
        self.check_cleanup()


if __name__ == "__main__":
    harness.main(Workflow(parse_args()))
