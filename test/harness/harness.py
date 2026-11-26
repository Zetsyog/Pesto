"""Shared helpers for the differential test scripts (stdlib only).

Every case owns a work directory. Each command is logged to commands.log with
its stdout/stderr kept in <phase>.out/<phase>.err, so a failing case can be
replayed by hand. The first failure is classified and written to result.txt
("<status> <detail>"), which the probe report collects.

Statuses: transform-fail, maxcut-fail, baseline-fail, compile-fail, crash,
          sanitizer, timeout, mismatch, pass, pass-untiled, harness-error.
"""

import argparse
import hashlib
import os
import re
import shlex
import signal
import subprocess
import sys
import traceback
from pathlib import Path

SANITIZER_REPORT = re.compile(r"AddressSanitizer|LeakSanitizer|runtime error:")
# TpzTiling: non-fatal warning printed (two lines) when a front is small.
TPZ_WARNING = re.compile(r"Warning: target volume too small\.\n\(TARGET_VOL_L1[^\n]*\n")

SANITIZER_ENV = {
    "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
    "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
}


def cmake_bool(value):
    """Interpret a CMake boolean (ON/OFF, TRUE/FALSE, 1/0, ...)."""
    return str(value).strip().upper() not in ("", "0", "OFF", "FALSE", "NO", "N", "IGNORE", "NOTFOUND")


def cflags(sanitizers):
    flags = ["-O2", "-g", "-fopenmp", "-fno-omit-frame-pointer", "-ffp-contract=off", "-w"]
    if sanitizers:
        flags += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"]
    return flags


def spawn(cmd, cwd, timeout, stdout=None, stderr=None, env=None):
    """Run cmd in its own process group; return its exit code, or None on timeout.

    On timeout the whole group gets SIGTERM, then SIGKILL 5 s later: pesto
    forks helpers that killing only the direct child would leave running.
    A negative code is a signal number, as in subprocess.
    """
    proc = subprocess.Popen([str(c) for c in cmd], cwd=cwd, stdout=stdout, stderr=stderr,
                            env=env, start_new_session=True)
    try:
        return proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        _kill_group(proc.pid, signal.SIGTERM)
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            _kill_group(proc.pid, signal.SIGKILL)
            proc.wait()
        return None
    finally:
        # Helpers that outlive their parent must not leak into the next phase.
        _kill_group(proc.pid, signal.SIGKILL)


def _kill_group(pgid, sig):
    try:
        os.killpg(pgid, sig)
    except (ProcessLookupError, PermissionError):
        pass


def describe_status(code):
    if code is None:
        return "timeout"
    if code < 0:
        try:
            return f"signal {signal.Signals(-code).name}"
        except ValueError:
            return f"signal {-code}"
    return str(code)


class CaseFailed(Exception):
    pass


class Case:
    def __init__(self, work_dir, name=None):
        self.dir = Path(work_dir).resolve()
        self.name = name or str(self.dir)
        self.phase_timeout = 60
        self.dir.mkdir(parents=True, exist_ok=True)
        os.environ.update(SANITIZER_ENV)
        os.environ.update({
            "TMPDIR": str(self.dir),
            "OMP_DYNAMIC": "FALSE",
            "OMP_THREAD_LIMIT": "4",
            "OMP_PROC_BIND": "FALSE",
        })
        (self.dir / "commands.log").write_text("")
        # Overwritten on completion; left as is when ctest kills the case.
        self._write_result("timeout killed before completion")

    def path(self, name):
        return self.dir / name

    def _write_result(self, line):
        (self.dir / "result.txt").write_text(line + "\n")

    def _log(self, line):
        with open(self.dir / "commands.log", "a") as log:
            log.write(line + "\n")

    def fail(self, status, detail):
        """Record the outcome and stop the case."""
        self._write_result(f"{status} {detail}")
        raise CaseFailed(f"{self.name}: {status}: {detail}\nArtifacts: {self.dir}")

    def passed(self, status="pass"):
        leftovers = sorted(self.dir.glob(".pesto-*"))
        if leftovers:
            self.fail("crash", "temporary workspace leak: " + " ".join(map(str, leftovers)))
        self._write_result(status)

    def run(self, phase, failure, cmd, threads=None, timeout=None):
        """Run one command. A timeout or a sanitizer report overrides
        <failure>, so the probe can tell a miscompiled program from a
        crashing one."""
        cmd = [str(c) for c in cmd]
        self._log(f"{phase}: {shlex.join(cmd)}")
        env = None
        if threads is not None:
            env = dict(os.environ, OMP_NUM_THREADS=str(threads))
        out_path, err_path = self.path(f"{phase}.out"), self.path(f"{phase}.err")
        with open(out_path, "wb") as out, open(err_path, "wb") as err:
            code = spawn(cmd, self.dir, timeout or self.phase_timeout, out, err, env)
        if code == 0:
            return
        status = describe_status(code)
        errors = err_path.read_bytes()[:4000].decode(errors="replace")
        if code is None:
            failure = "timeout"
        elif SANITIZER_REPORT.search(errors):
            failure = "sanitizer"
        self._log(f"  -> {status}")
        self.fail(failure, f"{phase} ({status}): see {phase}.err\n{errors}")

    def pipeline_args(self, pipeline_dir, name, tile):
        """Instantiate pipelines/<name>.json.in into the work dir; return the
        --config arguments (none for the default pipeline)."""
        if name == "default":
            return []
        template = (Path(pipeline_dir) / f"{name}.json.in").read_text()
        self.path("pipeline.json").write_text(template.replace("@TILE@", str(tile)))
        return ["--config", str(self.path("pipeline.json"))]


def dump_hash(path):
    """SHA256 of a result dump, without the diagnostics that transformed
    programs may legitimately print on stderr."""
    content = Path(path).read_bytes().decode(errors="surrogateescape")
    content = TPZ_WARNING.sub("", content)
    return hashlib.sha256(content.encode(errors="surrogateescape")).hexdigest()


def sha256_file(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main(body):
    """Run body(); turn a recorded failure into exit code 1."""
    try:
        body()
    except CaseFailed as failure:
        print(failure, file=sys.stderr)
        sys.exit(1)


def case_main(case_factory, body):
    """Like main(), for scripts that own a Case: an unexpected exception is a
    harness bug, recorded as harness-error instead of a misleading timeout."""
    case = case_factory()
    try:
        body(case)
    except CaseFailed as failure:
        print(failure, file=sys.stderr)
        sys.exit(1)
    except Exception:
        trace = traceback.format_exc()
        (case.dir / "result.txt").write_text(f"harness-error {trace.splitlines()[-1]}\n")
        print(trace, file=sys.stderr)
        sys.exit(1)


def common_parser(description):
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("--pesto", required=True)
    parser.add_argument("--cc", required=True)
    parser.add_argument("--sanitizers", type=cmake_bool, default=True)
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    return parser
