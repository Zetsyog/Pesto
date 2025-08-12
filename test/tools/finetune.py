import sys
import argparse
import bisect
import subprocess
import itertools
import logging
import hashlib
import os
from pathlib import Path
from typing import Optional, Generic, TypeVar
from collections.abc import Iterator

logger = logging.getLogger("finetune")

T = TypeVar("T")


class FixedSizeSortedList(Generic[T]):
    """A fixed-size sorted list that maintains the smallest elements."""

    def __init__(self, max_size):
        self.max_size = max_size
        self.data: list[T] = []

    def insert(self, value: T):
        """Insert a value into the sorted list, maintaining the size limit."""
        # Insert the value in sorted order
        bisect.insort(self.data, value, key=lambda x: x[0])
        # Remove the largest value if the size limit is exceeded
        if len(self.data) > self.max_size:
            self.data.pop()  # Remove the last (largest) element

    def __repr__(self):
        return repr(self.data)


class FTParameter:
    """Base class for a benchmarking parameter."""

    def __init__(self, name: str):
        self.name = name


class FTParameterRange(FTParameter):
    """Parameter that has a range of values to search for."""

    def __init__(self, name: str, min_value: int, max_value: int):
        super().__init__(name)
        self.min_value = min_value
        self.max_value = max_value


class FTParameterMinimize(FTParameter):
    """Parameter that the experiment will try to minimize."""

    def __init__(self, name: str, min_value: int):
        super().__init__(name)
        self.init_value = min_value


class FTParamInstance:
    """Instance of a parameter with a specific value.
    This is used to represent a specific configuration of parameters for a run.
    For example, it can represent an explicit parameter value for a FTParameterRange
    """

    def __init__(self, name: str, value: int):
        self.name = name
        self.value = value

    def __repr__(self):
        return f"{self.name}={self.value}"


class FTOptions:
    """Global options for the finetune script."""

    def __init__(self):
        self.parser = self._init_parser()

        self.sources: list[Path] = []
        self.log_output: Optional[Path] = None
        self.outputs: list[Path] = []
        self.pluto_enabled: bool = False
        self.pluto_bin: str = "polycc"
        self.parameters: list[FTParameter] = []
        self.compiler_binary: Path = Path("gcc")
        self.compile_extra_flags: list[str] = []
        self.top_n_runs: int = 20
        self.output_dump_baseline_sources: list[Path] = []

    def _init_parser(self) -> argparse.ArgumentParser:
        parser = argparse.ArgumentParser(
            description="Finetune parameters for a benchmark to find best performance."
        )
        parser.add_argument(
            "sources",
            default=None,
            nargs="*",
            type=Path,
            metavar="PATH",
            help="Path to the source file to be compiled and run.",
        )

        parser.add_argument(
            "--log-file",
            action="store",
            default=None,
            type=Path,
            required=False,
            metavar="LOG_FILE",
            dest="log_output",
            help="Path to the log file where the output will be written.",
        )

        parser.add_argument(
            "-o",
            "--output",
            action="append",
            nargs=1,
            default=[],
            type=Path,
            metavar="PATH",
            dest="outputs",
            help="Path to the output files where the results will be written."
            "Can be specified multiple times. Files can be json or csv.",
        )

        parser.add_argument(
            "--pluto",
            nargs="?",
            const=True,
            default=None,
            type=str,
            metavar="PLUTO_CMD",
            dest="pluto",
            help="Finetuning is done using Pluto.",
        )

        parser.add_argument(
            "--param-range",
            action="append",
            nargs=3,
            default=[],
            type=str,
            metavar=("NAME", "MIN", "MAX"),
            dest="param_ranges",
            help="Range of tile sizes to search for. ",
        )

        parser.add_argument(
            "--param-minimize",
            action="append",
            nargs=2,
            default=[],
            type=str,
            metavar=("NAME", "INIT_VALUE"),
            dest="param_minimize",
            help="Parameters to minimize during the search. ",
        )

        parser.add_argument(
            "--compile-extra-flags",
            action="extend",
            nargs="+",
            default=[],
            type=str,
            metavar="FLAG",
            dest="compile_extra_flags",
            help="Extra flags to pass to the compiler when compiling the source file. ",
        )

        parser.add_argument(
            "-I",
            "--include-dir",
            action="extend",
            nargs="+",
            default=[],
            type=Path,
            metavar="PATH",
            dest="include_dirs",
            help="Additional include directories for the compiler. "
            "Can be specified multiple times.",
        )

        parser.add_argument(
            "--output-dump-baseline",
            action="extend",
            nargs="+",
            default=[],
            type=Path,
            metavar="PATH",
            dest="output_dump_baseline",
            help="Path to the the source(s) file(s) that will be used as "
            "a baseline for output comparison. ",
        )

        parser.add_argument(
            "--compiler-bin",
            type=str,
            default=None,
            metavar="BIN",
            dest="compiler_binary",
            help="Path to the compiler binary to use for compilation. "
            "If not specified, the default compiler [gcc] will be used.",
        )

        return parser

    def parse(self) -> None:
        """Parse the command line arguments and initialize the options."""

        ns = self.parser.parse_args()

        self.sources = ns.sources
        self.log_output = ns.log_output
        self.outputs = ns.outputs

        for param in ns.param_ranges:
            name, min_value, max_value = param
            try:
                min_value = int(min_value)
                max_value = int(max_value)
            except ValueError as e:
                logger.error("Invalid parameter range: %s", e)
                sys.exit(1)

            # check if a parameter with the same name already exists
            if any(p.name == name for p in self.parameters):
                logger.error("Parameter '%s' is already defined.", name)
                sys.exit(1)

            self.parameters.append(FTParameterRange(name, min_value, max_value))

        for param in ns.param_minimize:
            name, min_value = param
            try:
                min_value = int(min_value)
            except ValueError as e:
                logger.error("Invalid parameter minimize: %s", e)
                sys.exit(1)

            # check if a parameter with the same name already exists
            if any(p.name == name for p in self.parameters):
                logger.error("Parameter '%s' is already defined.", name)
                sys.exit(1)

            self.parameters.append(FTParameterMinimize(name, min_value))

        if ns.include_dirs:
            for include_dir in ns.include_dirs:
                if not include_dir.exists():
                    logger.error("Include directory '%s' does not exist.", include_dir)
                    sys.exit(1)
                self.compile_extra_flags.extend(["-I", f"{include_dir.absolute()}"])

        if ns.compile_extra_flags:
            self.compile_extra_flags.extend(ns.compile_extra_flags)

        self.output_dump_baseline_sources.extend(ns.output_dump_baseline)
        # check all output dump baseline sources exist
        for src in self.output_dump_baseline_sources:
            if not src.exists():
                logger.error(
                    "Output dump baseline '%s' does not exist.",
                    src,
                )
                sys.exit(1)

        if ns.compiler_binary:
            self.compiler_binary = ns.compiler_binary

        # check if the given option is a valid bash command
        try:
            result = subprocess.run(
                [self.compiler_binary, "--version"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=True,
            )
            if result.returncode != 0:
                logger.error(
                    "Compiler '%s' is not valid: %s",
                    self.compiler_binary,
                    result.stderr,
                )
                sys.exit(1)
            logger.info(
                "Using compiler '%s': %s", self.compiler_binary, result.stdout.strip()
            )
        except (subprocess.CalledProcessError, FileNotFoundError) as e:
            logger.error(
                "Compiler '%s' is not valid or not found: %s", self.compiler_binary, e
            )
            sys.exit(1)

        # pluto stuff
        self.pluto_enabled = ns.pluto is not None
        if ns.pluto is not None and ns.pluto is not True:
            self.pluto_bin = ns.pluto
        # check if pluto is available
        try:
            result = subprocess.run(
                [self.pluto_bin, "--version"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
            # result code of polycc --version is 1 so no check for it
            # we could check the output but for now we just check that it runs
        except FileNotFoundError:
            logger.error(
                "Pluto binary '%s' not found. Please install Pluto or specify a valid path.",
                self.pluto_bin,
            )
            sys.exit(1)
        except subprocess.CalledProcessError as e:
            logger.error("Pluto execution '%s' failed: %s", self.pluto_bin, e)
            sys.exit(1)
        logger.info(
            "Using Pluto binary '%s': %s", self.pluto_bin, result.stdout.strip()
        )

    def dump(self, level: int = logging.INFO) -> None:
        """Dump the parsed options to the logger."""
        logger.log(level, "Parsed options:")
        logger.log(level, "\tSources: %s", self.sources)
        logger.log(level, "\tLog output: %s", self.log_output)
        logger.log(level, "\tPluto: %s", self.pluto_enabled)
        logger.log(level, "\tOutputs: %s", self.outputs)
        logger.log(level, "\tParameters:")
        for param in self.parameters:
            if isinstance(param, FTParameterRange):
                logger.log(
                    level,
                    "\t\t%s:\t[%d, %d]",
                    param.name,
                    param.min_value,
                    param.max_value,
                )
            elif isinstance(param, FTParameterMinimize):
                logger.log(level, "\t\t%s:\t<%d", param.name, param.init_value)
            else:
                logger.log(level, "\t\t%s", param.name)
        logger.log(level, "\tCompile extra flags: %s", self.compile_extra_flags)
        logger.log(level, "\tTop N runs: %d", self.top_n_runs)


class FTRun:
    """FTRun represents a single run of the benchmark with a given set of parameters.
    It can have a compile step if the benchmark needs to be compiled with the given parameters.
    Other processing steps can be added. Thoses steps are executed for each run. This means that
    those steps should depend on the parameters.
    Else they should be moved to the FTExperiment class.
    """

    def __init__(self, params: tuple[FTParamInstance, ...]):
        self.params: tuple[FTParamInstance, ...] = params

        self.has_compile_step: bool = False
        self.compile_done: bool = False
        self.compile_cmd: list[str] = "clang -march=native -O3 -fopenmp".split()
        self.compile_sources: list[Path] = []
        self.compile_extra_flags: list[str] = []

        self.binary: Optional[Path] = None

        self.exit_code: Optional[int] = None
        self.kernel_execution_time: Optional[float] = None

        self.stderr_fd: Optional[int] = None

    def add_compile_step(
        self, sources: Path, extra_flags: Optional[list[str]] = None
    ) -> None:
        """Set whether the run has a compile step"""
        self.has_compile_step = True
        self.compile_sources = sources
        if extra_flags is not None:
            self.compile_cmd.extend(extra_flags)
        self.binary = (sources[-1].parent / (sources[-1].stem + ".bin")).absolute()

    def _compile_step(self) -> None:
        """Compile the benchmark with the given parameters"""

        if self.compile_done:
            logger.debug("Compile step already done. Skipping.")
            return

        cmd = self.compile_cmd.copy()
        cmd.extend([str(s.absolute()) for s in self.compile_sources])
        cmd.append("-o")
        cmd.append(str(self.binary.absolute()))
        cmd.extend(self.compile_extra_flags)
        for param in self.params:
            cmd.append(f"-D{param.name}={param.value}")
        logger.debug("Compiling with command: %s", " ".join(cmd))
        result = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
        )
        try:
            stdout, stderr = result.communicate()
            if result.returncode != 0:
                logger.error("Compilation failed:\n%s\n%s", stdout, stderr)
                logger.error("Compile command: %s", " ".join(cmd))
                return
        except subprocess.SubprocessError as e:
            logger.error("Failed to compile binary: %s", e)
            logger.error("Compile command: %s", " ".join(cmd))
            return
        if not self.binary.exists():
            logger.error("Compilation failed: Binary does not exist.")
            logger.error("Compile command: %s", " ".join(cmd))

        logger.debug("Compilation succeeded:\n%s", stdout)

    def run(self) -> None:
        """Execute the run
        An FTRun is a single run of the benchmark with a given set of parameters
        """
        if self.has_compile_step:
            self._compile_step()

        self.exit_code = None
        self.kernel_execution_time = None

        if not self.binary or not self.binary.exists():
            logger.error("Binary does not exist. Cannot run the benchmark.")
            return

        result = subprocess.Popen(
            [str(self.binary.absolute())],
            stdout=subprocess.PIPE,
            stderr=self.stderr_fd,
            text=True,
        )
        try:
            stdout, _ = result.communicate()

            self.exit_code = result.returncode

            # parse the output to extract kernel execution time
            for line in stdout.splitlines():
                try:
                    self.kernel_execution_time = float(line)
                    break
                except ValueError:
                    self.kernel_execution_time = None

        except subprocess.SubprocessError as e:
            logger.error("Failed to run binary: %s", e)

    def cleanup(self) -> None:
        """Cleanup the run
        This will remove any file generated by the run, such as output files.
        """
        if self.binary.exists():
            self.binary.unlink()


class FTExperiment:
    """FTExperiment represents a single experiment that will finetune the benchmark parameters
    and run the benchmark with the given parameters.
    It will also perform output comparison with the baseline to ensure correctness of results.
    """

    def __init__(self):
        self.best_runs: FixedSizeSortedList[
            tuple[float, tuple[FTParamInstance, ...]]
        ] = FixedSizeSortedList(max_size=20)

        self._run_number: int = 5
        self._median_runs: int = 3
        self.trimmed_mean_results: list[tuple[float, tuple[FTParamInstance, ...]]] = []

    def _gen_range_param_instances(
        self, param_list: list[FTParameterRange]
    ) -> Iterator[tuple[FTParamInstance, ...]]:
        """
        Generate all possible parameter instances for the given range parameters.
        This will generate a Cartesian product of the ranges.
        """

        def powers_of_two_in_range(min_value, max_value):
            """Yield all powers of two in [min_value, max_value] (inclusive)."""
            v = 1
            while v <= max_value:
                if v >= min_value:
                    yield v
                v <<= 1

        range_product = []
        for param in param_list:
            values = powers_of_two_in_range(param.min_value, param.max_value)
            instance_list = []
            for value in values:
                instance_list.append(FTParamInstance(param.name, value))
            range_product.append(instance_list)

        # Generate the Cartesian product of the parameter instances
        return itertools.product(*range_product)

    def _gen_param_instances(
        self, param_list: list[FTParameter]
    ) -> Iterator[tuple[FTParamInstance, ...]]:
        """
        Generate all possible parameter instances for the given parameters.
        This will generate a Cartesian product of the parameters.
        """
        range_params_instances = self._gen_range_param_instances(
            [p for p in param_list if isinstance(p, FTParameterRange)]
        )

        minimize_params = [p for p in param_list if isinstance(p, FTParameterMinimize)]
        if minimize_params:
            # For each combination, append the minimize param(s) with their initial value
            def with_minimize_params(instances):
                for combo in instances:
                    combo_list = list(combo)
                    for min_param in minimize_params:
                        combo_list.append(
                            FTParamInstance(min_param.name, min_param.init_value)
                        )
                    yield tuple(combo_list)

            return with_minimize_params(range_params_instances)

        return range_params_instances

    def _gen_pluto_sources(
        self, options: FTOptions, param_instances: tuple[FTParamInstance, ...]
    ) -> list[Path]:
        """
        For each source file, apply pluto transformations and return the newly generated source files.
        This will create new source files with the given parameters.
        The new source files will be named <original_name>.pluto.c.
        If the source file is not a C file, it will be skipped.
        If the source file does not contain any scops, it will be skipped.
        """

        gen_sources = []
        for src in options.sources:
            if not src.is_file():
                logger.warning("Source file '%s' does not exist. Skipping.", src)
                continue

            # Check if the file contains at least one #pragma scop directive
            has_scop = False
            with open(src, "r", encoding="utf-8") as f:
                for line in f:
                    if "#pragma scop" in line:
                        has_scop = True
                        break
            if not has_scop:
                gen_sources.append(src)
                continue

            output_file = src.with_suffix(".pluto.c")
            tile_size_file = Path("tile.sizes")

            if tile_size_file.exists():
                tile_size_file.unlink()

            # Apply pluto on this source file
            pluto_cmd = [
                options.pluto_bin,
                str(src.absolute()),
                "-o",
                str(output_file.absolute()),
            ]
            # TODO: custom pluto flags
            pluto_cmd.extend(["--tile", "--nounroll", "--parallel"])

            tile_parameters_len = len(
                [p for p in param_instances if p.name.startswith("T")]
            )
            tile_sizes = [32] * tile_parameters_len
            # extract all parameters that have a name like "T[0-9]"
            for param in param_instances:
                if param.name.startswith("T"):
                    if param.name[1:].isdigit():
                        tile_sizes[int(param.name[1:])] = param.value

            with open(tile_size_file, "w", encoding="utf-8") as f:
                for size in tile_sizes:
                    f.write(f"{size}\n")

            # call pluto
            res = subprocess.run(
                pluto_cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

            if tile_size_file.exists():
                tile_size_file.unlink()

            if res.returncode != 0:
                logger.error(
                    "Failed to call pluto '%s': %s",
                    " ".join(pluto_cmd),
                    res.stdout.strip() + "\n" + res.stderr.strip(),
                )
                continue

            if not output_file.exists():
                logger.error(
                    "Pluto did not generate output file '%s' for source '%s'.",
                    output_file,
                    src,
                )
                continue
            gen_sources.append(output_file)

            logger.debug(
                "Generated pluto source file '%s' from '%s'.",
                output_file.absolute(),
                src,
            )

        return gen_sources

    def _get_program_sources(
        self, options: FTOptions, param_instances: tuple[FTParamInstance, ...]
    ) -> list[Path]:
        if options.pluto_enabled:
            # If pluto is enabled, we need to generate the pluto sources
            return self._gen_pluto_sources(options, param_instances)
        return options.sources

    def _cleanup_pluto_sources(self, sources: list[Path]) -> None:
        """Cleanup the pluto sources generated by the experiment."""
        for src in sources:
            if src.suffix == ".pluto.c":
                try:
                    src.unlink()
                except OSError as e:
                    logger.error("Failed to remove pluto source '%s': %s", src, e)

    def _finetune(self, options: FTOptions) -> None:
        """
        Finetune the benchmark with the given options.
        Perform an exhaustive search over the given parameters (essentially tiles sizes)
        """

        param_instances = self._gen_param_instances(options.parameters)
        extra_flags = options.compile_extra_flags + ["-DPOLYBENCH_TIME"]

        logger.info(
            "\t%s%s",
            " ".join(f"{param.name[:15]:<15}" for param in options.parameters),
            "score",
        )
        for param_instance in param_instances:
            sources = self._get_program_sources(options, param_instance)

            ftrun = FTRun(param_instance)
            ftrun.add_compile_step(sources, extra_flags=extra_flags)
            ftrun.run()
            ftrun.cleanup()

            if options.pluto_enabled:
                self._cleanup_pluto_sources(sources)

            score_str = ""

            if ftrun.exit_code != 0:  # non-zero exit code indicates an error
                score_str += f"N/A (exit code: {ftrun.exit_code})"
            elif ftrun.kernel_execution_time is None:
                # no execution time found in stdout
                score_str += "N/A (no execution time)"
            else:  # valid run with execution time
                # add execution time to log
                score_str += f"{ftrun.kernel_execution_time:.6f}"

                # keep track of the best runs
                self.best_runs.insert((ftrun.kernel_execution_time, ftrun.params))

            logger.info(
                "\t%s%s",
                " ".join(f"{str(p.value)[:15]:<15}" for p in ftrun.params),
                score_str,
            )

    def _measure_perf(self, options: FTOptions) -> None:
        """Re-run the best runs multiple times to get a better average performance."""
        logger.info(
            "\t%s%s%s",
            " ".join(f"{p.name[:15]:<15}" for p in options.parameters),
            f"{"score":<15}",
            f"{"Trimmed Mean":<15}",
        )
        self.trimmed_mean_results.clear()

        for score, param_instances in self.best_runs.data:

            score_list = []

            sources = self._get_program_sources(options, param_instances)

            ftrun = FTRun(param_instances)
            ftrun.add_compile_step(
                sources,
                extra_flags=options.compile_extra_flags + ["-DPOLYBENCH_TIME"],
            )

            # run the best runs multiple times to get a better average
            for _ in range(self._run_number):
                ftrun.run()

                if ftrun.exit_code != 0:
                    logger.error("Run failed with exit code: %d", ftrun.exit_code)
                    break

                if ftrun.kernel_execution_time is None:
                    logger.error("No execution time found in output.")
                    break

                score_list.append((ftrun.kernel_execution_time, ftrun.params))

            ftrun.cleanup()

            if options.pluto_enabled:
                self._cleanup_pluto_sources(sources)

            if len(score_list) < self._run_number:
                logger.error(
                    "Not enough runs completed. Expected %d, got %d.",
                    self._run_number,
                    len(score_list),
                )
                logger.error("An error may have occurred during the runs. Skipping.")
                continue

            # sort the scores and compute the mean of the median runs
            score_list.sort(key=lambda x: x[0])
            median_scores = score_list[
                len(score_list) // 2
                - self._median_runs // 2 : len(score_list) // 2
                + self._median_runs // 2
            ]
            mean_score = sum(score[0] for score in median_scores) / len(median_scores)
            self.trimmed_mean_results.append((mean_score, param_instances))

            logger.info(
                "\t%s%s%s",
                " ".join(f"{p.value:<15}" for p in param_instances),
                f"{score:<15.6f}",
                f"{mean_score:<15.6f}",
            )

        # Sort the trimmed mean results by score
        self.trimmed_mean_results.sort(key=lambda x: x[0])

    def _compare_output_with_baseline(self, options: FTOptions) -> None:
        """Compare the output of the runs with the baseline output."""
        if not options.output_dump_baseline_sources:
            logger.error("No baseline output specified for comparison.")
            return

        # First, we need to run the baseline output to generate the expected output
        baseline_run = FTRun(tuple())
        baseline_run.add_compile_step(
            options.output_dump_baseline_sources,
            extra_flags=options.compile_extra_flags
            + ["-DPOLYBENCH_TIME", "-DPOLYBENCH_DUMP_ARRAYS"],
        )

        # Redirect stderr to a file to capture the output
        stderr_file = open("__stderr.dump", "w", encoding="utf-8")
        baseline_run.stderr_fd = stderr_file.fileno()

        baseline_run.run()
        # Check if program ran successfully
        if baseline_run.exit_code != 0:
            logger.error(
                "Baseline run failed with exit code: %d", baseline_run.exit_code
            )
            return

        stderr_file.close()

        # Compute the hash of the baseline output
        baseline_hash = None
        with open("__stderr.dump", "rb") as f:
            baseline_hash = hashlib.sha256(f.read()).hexdigest()

        if not baseline_hash:
            logger.error("Failed to compute hash of the baseline output.")
            return

        if os.path.exists("__stderr.dump"):
            os.remove("__stderr.dump")
        baseline_run.cleanup()

        logger.info("Baseline output hash: %s", baseline_hash)

        # Compare the output of the best runs with the baseline output
        # We stop comparing after the first match
        # We want to find the run that has the best performance and matches the baseline output
        for _, param_instances in self.trimmed_mean_results:
            sources = self._get_program_sources(options, param_instances)

            ftrun = FTRun(param_instances)
            ftrun.add_compile_step(
                options.sources,
                extra_flags=options.compile_extra_flags
                + ["-DPOLYBENCH_TIME", "-DPOLYBENCH_DUMP_ARRAYS"],
            )

            # Redirect stderr to a file to capture the output
            stderr_file = open("__stderr.dump", "w", encoding="utf-8")
            ftrun.stderr_fd = stderr_file.fileno()

            ftrun.run()

            if ftrun.exit_code != 0:
                logger.error("Run failed with exit code: %d", ftrun.exit_code)
                continue

            stderr_file.close()

            # Compute the hash of the output
            run_hash = None
            with open("__stderr.dump", "rb") as f:
                run_hash = hashlib.sha256(f.read()).hexdigest()

            if not run_hash:
                logger.error("Failed to compute hash of the run output.")
                continue

            if os.path.exists("__stderr.dump"):
                os.remove("__stderr.dump")

            ftrun.cleanup()
            if options.pluto_enabled:
                self._cleanup_pluto_sources(sources)

            logger.info("Run output hash: %s", run_hash)

            if run_hash == baseline_hash:
                logger.info(
                    "Run with parameters {%s} is correct.",
                    ", ".join(f"{p.name}={p.value}" for p in param_instances),
                )
                break

    def start(self, options: FTOptions) -> None:
        """Start the given experiment
        An experiment will first try to finetune the benchmark parameters
        and then run the benchmark with the given parameters.
        Then it will perform output comparison with the baseline to ensure correctness of results.
        This is the main entry point for starting an experiment.

        Args:
            options (FTOptions): _description_
        """

        # do finetuning
        logger.info("Starting finetuning...")
        self._finetune(options)

        # then re-run the best runs but multiple times to get a better average
        # allows to get a better estimate of the performance
        logger.info("Measuring performance of the best runs...")
        self._measure_perf(options)

        if options.output_dump_baseline_sources:
            logger.info("Comparing output with baseline...")
            self._compare_output_with_baseline(options)


def main():
    """Main entry point for the finetune script."""
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
        handlers=[logging.StreamHandler(sys.stderr)],
    )
    options = FTOptions()
    options.parse()
    if options.log_output:
        logger.addHandler(logging.FileHandler(options.log_output))
    logger.info("Starting finetune script...")

    options.dump()

    experiment = FTExperiment()
    logger.info("Starting experiment...")
    experiment.start(options)

    logger.info("finetune done.")


if __name__ == "__main__":
    main()
