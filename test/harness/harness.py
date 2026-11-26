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
from collections.abc import Callable, Mapping, Sequence
from pathlib import Path
from typing import IO, NoReturn, TypeAlias

# One command-line argument; everything is passed through str().
CmdArg: TypeAlias = "str | os.PathLike[str] | int"

SANITIZER_REPORT = re.compile(r"AddressSanitizer|LeakSanitizer|runtime error:")
# TpzTiling: non-fatal warning printed (two lines) when a front is small.
TPZ_WARNING = re.compile(r"Warning: target volume too small\.\n\(TARGET_VOL_L1[^\n]*\n")

SANITIZER_ENV: dict[str, str] = {
    "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
    "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
}


class CommonArgs(argparse.Namespace):  # pylint: disable=too-few-public-methods
    """Arguments shared by the scripts built on common_parser()."""

    pesto: str
    cc: str
    sanitizers: bool
    source_dir: Path
    work_dir: Path


def cmake_bool(value: str) -> bool:
    """Interpret a CMake boolean (ON/OFF, TRUE/FALSE, 1/0, ...)."""
    falsy = ("", "0", "OFF", "FALSE", "NO", "N", "IGNORE", "NOTFOUND")
    return value.strip().upper() not in falsy


def cflags(sanitizers: bool) -> list[str]:
    """C compiler flags shared by the baseline and the transformed program."""
    flags = ["-O2", "-g", "-fopenmp", "-fno-omit-frame-pointer", "-ffp-contract=off", "-w"]
    if sanitizers:
        flags += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"]
    return flags


def spawn(  # pylint: disable=too-many-arguments
    cmd: Sequence[CmdArg],
    cwd: Path,
    timeout: float,
    *,
    stdout: IO[bytes] | None = None,
    stderr: IO[bytes] | None = None,
    env: Mapping[str, str] | None = None,
) -> int | None:
    """Run cmd in its own process group; return its exit code, or None on timeout.

    On timeout the whole group gets SIGTERM, then SIGKILL 5 s later: pesto
    forks helpers that killing only the direct child would leave running.
    A negative code is a signal number, as in subprocess.
    """
    with subprocess.Popen([str(c) for c in cmd], cwd=cwd, stdout=stdout, stderr=stderr,
                          env=env, start_new_session=True) as proc:
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


def _kill_group(pgid: int, sig: signal.Signals) -> None:
    try:
        os.killpg(pgid, sig)
    except (ProcessLookupError, PermissionError):
        pass


def describe_status(code: int | None) -> str:
    """Human-readable form of a spawn() result."""
    if code is None:
        return "timeout"
    if code < 0:
        try:
            return f"signal {signal.Signals(-code).name}"
        except ValueError:
            return f"signal {-code}"
    return str(code)


class CaseFailed(Exception):
    """A test case failed; the message says why and where its artifacts are."""


class Case:
    """The work directory of one test case, and the commands run in it."""

    def __init__(self, work_dir: str | os.PathLike[str], name: str | None = None) -> None:
        self.dir: Path = Path(work_dir).resolve()
        self.name: str = name or str(self.dir)
        self.phase_timeout: float = 60
        self.dir.mkdir(parents=True, exist_ok=True)
        os.environ.update(SANITIZER_ENV)
        os.environ.update({
            "TMPDIR": str(self.dir),
            "OMP_DYNAMIC": "FALSE",
            "OMP_THREAD_LIMIT": "4",
            "OMP_PROC_BIND": "FALSE",
        })
        (self.dir / "commands.log").write_text("", encoding="utf-8")
        # Overwritten on completion; left as is when ctest kills the case.
        self._write_result("timeout killed before completion")

    def path(self, name: str) -> Path:
        """Path of a file in the work directory."""
        return self.dir / name

    def _write_result(self, line: str) -> None:
        (self.dir / "result.txt").write_text(line + "\n", encoding="utf-8")

    def _log(self, line: str) -> None:
        with open(self.dir / "commands.log", "a", encoding="utf-8") as log:
            log.write(line + "\n")

    def fail(self, status: str, detail: str) -> NoReturn:
        """Record the outcome and stop the case."""
        self._write_result(f"{status} {detail}")
        raise CaseFailed(f"{self.name}: {status}: {detail}\nArtifacts: {self.dir}")

    def passed(self, status: str = "pass") -> None:
        """Record a successful outcome, unless pesto leaked a temporary workspace."""
        leftovers = sorted(self.dir.glob(".pesto-*"))
        if leftovers:
            self.fail("crash", "temporary workspace leak: " + " ".join(map(str, leftovers)))
        self._write_result(status)

    def run(  # pylint: disable=too-many-arguments
        self,
        phase: str,
        failure: str,
        cmd: Sequence[CmdArg],
        *,
        threads: int | None = None,
        timeout: float | None = None,
    ) -> None:
        """Run one command. A timeout or a sanitizer report overrides
        <failure>, so the probe can tell a miscompiled program from a
        crashing one."""
        args = [str(c) for c in cmd]
        self._log(f"{phase}: {shlex.join(args)}")
        env = None
        if threads is not None:
            env = dict(os.environ, OMP_NUM_THREADS=str(threads))
        out_path, err_path = self.path(f"{phase}.out"), self.path(f"{phase}.err")
        with open(out_path, "wb") as out, open(err_path, "wb") as err:
            code = spawn(args, self.dir, timeout or self.phase_timeout,
                         stdout=out, stderr=err, env=env)
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

    def pipeline_args(self, pipeline_dir: Path, name: str, tile: int | str | None) -> list[str]:
        """Instantiate pipelines/<name>.json.in into the work dir; return the
        --config arguments (none for the default pipeline). <tile> fills
        @TILE@; parameter-tiling pipelines have none, their tile sizes are the
        DIVn macros of the generated code."""
        if name == "default":
            return []
        template = (pipeline_dir / f"{name}.json.in").read_text(encoding="utf-8")
        if "@TILE@" in template:
            if tile is None:
                raise ValueError(f"pipeline {name} has fixed tile sizes (@TILE@): pass a tile size")
            template = template.replace("@TILE@", str(tile))
        self.path("pipeline.json").write_text(template, encoding="utf-8")
        return ["--config", str(self.path("pipeline.json"))]


def dump_hash(path: Path) -> str:
    """SHA256 of a result dump, without the diagnostics that transformed
    programs may legitimately print on stderr."""
    content = path.read_bytes().decode(errors="surrogateescape")
    content = TPZ_WARNING.sub("", content)
    return hashlib.sha256(content.encode(errors="surrogateescape")).hexdigest()


def sha256_file(path: Path) -> str:
    """SHA256 of a file's raw bytes."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main(body: Callable[[], object]) -> None:
    """Run body(); turn a recorded failure into exit code 1."""
    try:
        body()
    except CaseFailed as failure:
        print(failure, file=sys.stderr)
        sys.exit(1)


def case_main(case_factory: Callable[[], Case], body: Callable[[Case], object]) -> None:
    """Like main(), for scripts that own a Case: an unexpected exception is a
    harness bug, recorded as harness-error instead of a misleading timeout."""
    case = case_factory()
    try:
        body(case)
    except CaseFailed as failure:
        print(failure, file=sys.stderr)
        sys.exit(1)
    except Exception:  # pylint: disable=broad-exception-caught  # any harness bug
        trace = traceback.format_exc()
        (case.dir / "result.txt").write_text(f"harness-error {trace.splitlines()[-1]}\n",
                                             encoding="utf-8")
        print(trace, file=sys.stderr)
        sys.exit(1)


def common_parser(description: str | None) -> argparse.ArgumentParser:
    """Parser for the arguments of CommonArgs."""
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("--pesto", required=True)
    parser.add_argument("--cc", required=True)
    parser.add_argument("--sanitizers", type=cmake_bool, default=True)
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    return parser
