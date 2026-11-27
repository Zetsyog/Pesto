# PolyBench/C and NPDP kernels as differential regression tests.
#
# * benchmarks.txt lists every kernel, its class, and the pipelines tested by ctest (labels
#   "regression;bench", part of check-regressions).
# * check-bench-probe runs every kernel through every pipeline of its class (outside the normal
#   ctest run) and writes bench-probe/probe-report.md: use it to grow the tested pipelines and to
#   spot tested pairs that broke.
option(PESTO_ENABLE_BENCH_REGRESSIONS "Run PolyBench and NPDP kernels as regression tests" ON)
if(NOT PESTO_ENABLE_BENCH_REGRESSIONS)
	return()
endif()

include(FetchContent)
# SOURCE_SUBDIR points nowhere: these are plain source trees, never built.
FetchContent_Declare(
	pesto_polybench
	URL https://downloads.sourceforge.net/project/polybench/polybench-c-4.2.1-beta.tar.gz
	URL_HASH SHA256=426519ee8443a5f2175de6a3e9328cda8917a5e33053d0e8f59855ab56d689a4
	SOURCE_SUBDIR do-not-build
	DOWNLOAD_EXTRACT_TIMESTAMP true
)
FetchContent_Declare(
	pesto_npdp
	GIT_REPOSITORY https://github.com/Zetsyog/NPDP_Bench.git
	GIT_TAG d8770b55c5ecb9de0ae0ce142b57dd69947386d5
	SOURCE_SUBDIR do-not-build
	DOWNLOAD_EXTRACT_TIMESTAMP true
)
FetchContent_MakeAvailable(pesto_polybench pesto_npdp)
if(NOT EXISTS "${pesto_polybench_SOURCE_DIR}/utilities/polybench.c"
   OR NOT EXISTS "${pesto_npdp_SOURCE_DIR}/include/mem.h"
)
	message(
		WARNING "PolyBench/NPDP sources are missing (FETCHCONTENT_FULLY_DISCONNECTED on a fresh "
				"build directory?): benchmark regressions are disabled. Configure once with "
				"-DFETCHCONTENT_FULLY_DISCONNECTED=OFF, or -DPESTO_ENABLE_BENCH_REGRESSIONS=OFF."
	)
	return()
endif()

set(PESTO_BENCH_PROBE_DIR "${CMAKE_BINARY_DIR}/bench-probe")
# Hand-tuned configurations: pipelines/handtuned/<suite>/<kernel name>.json.
set(PESTO_BENCH_HANDTUNED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/pipelines/handtuned")

# The benchmark table: one row per kernel, see benchmarks.txt for the format.
set(PESTO_BENCH_TABLE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks.txt")
file(READ "${PESTO_BENCH_TABLE_FILE}" PESTO_BENCH_TABLE)
# Re-configure when the table changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PESTO_BENCH_TABLE_FILE}")

# Pipelines probed for each <suite>_<class>, before handtuned/pluto are added.
set(PESTO_BENCH_CLASS_PIPELINES_polybench_algebraic default pluto-like algebraic hybrid)
set(PESTO_BENCH_CLASS_PIPELINES_polybench_trapezoidal default pluto-like tpz)
set(PESTO_BENCH_CLASS_PIPELINES_polybench_unsupported "")
set(PESTO_BENCH_CLASS_PIPELINES_npdp_trapezoidal default pluto-like)

set(PESTO_BENCH_POLYCC
	""
	CACHE STRING "polycc command for the probe's Pluto reference pipeline (empty: disabled)"
)

# Extra polycc flags for the Pluto reference pipeline.
set(PESTO_BENCH_POLYCC_FLAGS_heat-3d --nocloogbacktrack)

# _pesto_bench_command(<suite> <kernel> <pipeline> <work-dir> <out-var>)
function(_pesto_bench_command suite kernel pipeline work_dir var)
	get_filename_component(name "${kernel}" NAME)
	if(suite STREQUAL "polybench")
		set(root "${pesto_polybench_SOURCE_DIR}")
	else()
		set(root "${pesto_npdp_SOURCE_DIR}")
	endif()
	# gen_maxcut.py needs sympy: run it without -I, so site packages are visible.
	set(${var}
		${PESTO_TEST_PYTHON}
		"${PESTO_TEST_HARNESS_DIR}/run_bench.py"
		--pesto
		"$<TARGET_FILE:pesto-cli>"
		--cc
		"${CMAKE_C_COMPILER}"
		--sanitizers
		"${PESTO_REGRESSION_SANITIZERS}"
		--suite
		"${suite}"
		--kernel
		"${kernel}"
		--bench-root
		"${root}"
		--pipeline
		"${pipeline}"
		--handtuned-config
		"${PESTO_BENCH_HANDTUNED_DIR}/${suite}/${name}.json"
		--source-dir
		"${CMAKE_CURRENT_SOURCE_DIR}"
		--work-dir
		"${work_dir}"
		--python
		"${Python3_EXECUTABLE}"
		--gen-maxcut
		"${PROJECT_SOURCE_DIR}/utilities/gen_maxcut.py"
		--iscc
		"${BARVINOK_INSTALL_DIR}/bin/iscc"
		"--polycc=${PESTO_BENCH_POLYCC}"
		"--polycc-flags=${PESTO_BENCH_POLYCC_FLAGS_${name}}"
		PARENT_SCOPE
	)
endfunction()

# pesto_bench(<suite> <kernel> PIPELINES <pipeline>...): allowlist entry.
function(pesto_bench suite kernel)
	cmake_parse_arguments(PARSE_ARGV 2 arg "" "" "PIPELINES")
	get_filename_component(name "${kernel}" NAME)
	foreach(pipeline IN LISTS arg_PIPELINES)
		set(test bench-${suite}-${name}-${pipeline})
		_pesto_bench_command(
			${suite} ${kernel} ${pipeline} "${PESTO_REGRESSION_ARTIFACT_DIR}/${test}" command
		)
		add_test(NAME ${test} COMMAND ${command})
		set_tests_properties(${test} PROPERTIES LABELS "regression;bench" TIMEOUT 900 PROCESSORS 2)
		set_property(
			DIRECTORY
			APPEND
			PROPERTY PESTO_BENCH_ALLOWLIST "${suite}/${name}/${pipeline}"
		)
	endforeach()
endfunction()

# _pesto_bench_rows(<out-var>): the rows of PESTO_BENCH_TABLE, as
# "suite|kernel|class|probed,pipelines|tested,pipelines" strings.
function(_pesto_bench_rows var)
	set(rows)
	# Drop comments before splitting into lines: they may hold ';' or '[', which
	# CMake lists would take as separators or brackets.
	string(REGEX REPLACE "#[^\n]*" "" table "${PESTO_BENCH_TABLE}")
	string(REPLACE "\n" ";" lines "${table}")
	foreach(line IN LISTS lines)
		string(STRIP "${line}" line)
		if(line STREQUAL "")
			continue()
		endif()
		string(REGEX REPLACE "[ \t]+" ";" fields "${line}")
		list(LENGTH fields nfields)
		if(nfields LESS 4)
			message(FATAL_ERROR "benchmarks.txt: malformed row '${line}' "
								"(expected: <suite> <kernel> <class> <tested pipelines...> or -)")
		endif()
		list(POP_FRONT fields suite kernel class)
		if(NOT DEFINED PESTO_BENCH_CLASS_PIPELINES_${suite}_${class})
			message(FATAL_ERROR "benchmarks.txt: unknown class '${class}' for suite '${suite}' "
								"in row '${line}'")
		endif()
		get_filename_component(name "${kernel}" NAME)

		set(probed ${PESTO_BENCH_CLASS_PIPELINES_${suite}_${class}})
		if(class STREQUAL "trapezoidal" AND EXISTS "${PESTO_BENCH_HANDTUNED_DIR}/${suite}/${name}.json")
			list(APPEND probed handtuned)
		endif()
		list(APPEND probed pluto) # filtered below when PESTO_BENCH_POLYCC is unset

		set(tested)
		if(NOT fields STREQUAL "-")
			foreach(pipeline IN LISTS fields)
				if(NOT pipeline IN_LIST probed)
					message(FATAL_ERROR "benchmarks.txt: ${suite}/${name}: '${pipeline}' is not "
										"probed for this kernel (probed: ${probed})")
				endif()
				list(APPEND tested ${pipeline})
			endforeach()
		endif()

		if(NOT PESTO_BENCH_POLYCC)
			list(REMOVE_ITEM probed pluto)
			list(REMOVE_ITEM tested pluto)
		endif()
		list(JOIN probed "," probed)
		list(JOIN tested "," tested)
		list(APPEND rows "${suite}|${kernel}|${class}|${probed}|${tested}")
	endforeach()
	set(${var} "${rows}" PARENT_SCOPE)
endfunction()

# The probe lives in its own ctest directory, so plain `ctest` never runs it.
function(_pesto_bench_probe rows)
	set(content "# Generated by test/regression/benchmarks.cmake\n")
	foreach(row IN LISTS rows)
		string(REPLACE "|" ";" row "${row}")
		list(GET row 0 suite)
		list(GET row 1 kernel)
		list(GET row 3 pipelines)
		string(REPLACE "," ";" pipelines "${pipelines}")
		get_filename_component(name "${kernel}" NAME)
		foreach(pipeline IN LISTS pipelines)
			set(test probe-${suite}-${name}-${pipeline})
			_pesto_bench_command(
				${suite} ${kernel} ${pipeline}
				"${PESTO_BENCH_PROBE_DIR}/cases/${suite}/${name}/${pipeline}" command
			)
			# Quoted arguments: CMake lists mishandle bracket arguments.
			set(line "add_test(${test}")
			foreach(arg IN LISTS command)
				string(REPLACE "\\" "\\\\" arg "${arg}")
				string(REPLACE "\"" "\\\"" arg "${arg}")
				string(APPEND line " \"${arg}\"")
			endforeach()
			string(APPEND content "${line})\n")
			string(APPEND content
				   "set_tests_properties(${test} PROPERTIES TIMEOUT 900 LABELS probe)\n"
			)
		endforeach()
	endforeach()
	file(
		GENERATE
		OUTPUT "${PESTO_BENCH_PROBE_DIR}/CTestTestfile.cmake"
		CONTENT "${content}"
	)
endfunction()

_pesto_bench_rows(PESTO_BENCH_ROWS)
_pesto_bench_probe("${PESTO_BENCH_ROWS}")

cmake_host_system_information(RESULT PESTO_BENCH_PROBE_JOBS QUERY NUMBER_OF_LOGICAL_CORES)
add_custom_target(
	check-bench-probe
	COMMAND
		${PESTO_TEST_PYTHON} ${PESTO_TEST_HARNESS_DIR}/probe_report.py --ctest
		${CMAKE_CTEST_COMMAND} --probe-dir ${PESTO_BENCH_PROBE_DIR} --jobs ${PESTO_BENCH_PROBE_JOBS}
	DEPENDS pesto-cli
	USES_TERMINAL
	COMMENT "Probing every PolyBench/NPDP kernel through every pipeline"
)

# Register the tested pipelines of every row.
foreach(row IN LISTS PESTO_BENCH_ROWS)
	string(REPLACE "|" ";" row "${row}")
	list(GET row 0 suite)
	list(GET row 1 kernel)
	list(GET row 4 tested)
	string(REPLACE "," ";" tested "${tested}")
	if(tested)
		pesto_bench(${suite} ${kernel} PIPELINES ${tested})
	endif()
endforeach()

# -----------------------------------------------------------------------------
get_property(
	allowlist
	DIRECTORY
	PROPERTY PESTO_BENCH_ALLOWLIST
)
list(JOIN allowlist "\n" allowlist)
file(WRITE "${PESTO_BENCH_PROBE_DIR}/allowlist.txt" "${allowlist}\n")
