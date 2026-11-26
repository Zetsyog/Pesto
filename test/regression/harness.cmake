# Shared helpers for the differential regression scripts (run.cmake, bench.cmake).
#
# Every case owns WORK_DIR. Each command is logged to commands.log with its
# stdout/stderr kept in <phase>.out/<phase>.err, so a failing case can be
# replayed by hand. The first failure is classified and written to result.txt
# ("<status> <detail>"), which the probe report collects.
#
# Statuses: transform-fail, maxcut-fail, baseline-fail, compile-fail, crash,
#           sanitizer, timeout, mismatch, pass, pass-untiled.

cmake_policy(VERSION 3.22)

file(MAKE_DIRECTORY "${WORK_DIR}")
set(ENV{TMPDIR} "${WORK_DIR}")
set(ENV{OMP_DYNAMIC} FALSE)
set(ENV{OMP_THREAD_LIMIT} 4)
set(ENV{OMP_PROC_BIND} FALSE)
set(ENV{ASAN_OPTIONS} "detect_leaks=0:halt_on_error=1")
set(ENV{UBSAN_OPTIONS} "halt_on_error=1:print_stacktrace=1")
file(WRITE "${WORK_DIR}/commands.log" "")
# Overwritten on completion; left as is when ctest kills the case.
file(WRITE "${WORK_DIR}/result.txt" "timeout killed before completion\n")

if(NOT DEFINED CASE_NAME)
    set(CASE_NAME "${WORK_DIR}")
endif()
find_program(HARNESS_TIMEOUT timeout)
if(NOT DEFINED PHASE_TIMEOUT)
    set(PHASE_TIMEOUT 60)
endif()

# harness_fail(<status> <detail>): record the outcome and stop the case.
function(harness_fail status detail)
    file(WRITE "${WORK_DIR}/result.txt" "${status} ${detail}\n")
    message(FATAL_ERROR "${CASE_NAME}: ${status}: ${detail}\nArtifacts: ${WORK_DIR}")
endfunction()

# harness_pass([detail]): record success.
function(harness_pass)
    set(status pass)
    if(ARGV0)
        set(status "${ARGV0}")
    endif()
    file(GLOB leftovers "${WORK_DIR}/.pesto-*")
    if(leftovers)
        harness_fail(crash "temporary workspace leak: ${leftovers}")
    endif()
    file(WRITE "${WORK_DIR}/result.txt" "${status}\n")
endfunction()

# harness_run(<phase> <failure-status> <command>...): run one command. A
# timeout or a sanitizer report overrides <failure-status>, so the probe can
# tell a miscompiled program from a crashing one.
function(harness_run phase failure)
    string(JOIN " " display ${ARGN})
    file(APPEND "${WORK_DIR}/commands.log" "${phase}: ${display}\n")
    # GNU timeout kills the whole process group: pesto forks helpers that a
    # plain execute_process TIMEOUT would leave running.
    set(command ${ARGN})
    math(EXPR outer_timeout "${PHASE_TIMEOUT} + 30")
    if(HARNESS_TIMEOUT)
        set(command "${HARNESS_TIMEOUT}" --kill-after=5 ${PHASE_TIMEOUT} ${ARGN})
    endif()
    execute_process(
        COMMAND ${command} WORKING_DIRECTORY "${WORK_DIR}"
        RESULT_VARIABLE status OUTPUT_FILE "${WORK_DIR}/${phase}.out"
        ERROR_FILE "${WORK_DIR}/${phase}.err" TIMEOUT ${outer_timeout}
    )
    if("${status}" STREQUAL "0")
        return()
    endif()
    file(READ "${WORK_DIR}/${phase}.err" errors LIMIT 4000)
    if(status MATCHES "timeout" OR status STREQUAL "124" OR status STREQUAL "137")
        set(failure timeout)
    elseif(errors MATCHES "AddressSanitizer|LeakSanitizer|runtime error:")
        set(failure sanitizer)
    endif()
    file(APPEND "${WORK_DIR}/commands.log" "  -> ${status}\n")
    harness_fail(${failure} "${phase} (${status}): see ${phase}.err\n${errors}")
endfunction()

# harness_dump_hash(<file> <var>): SHA256 of a result dump, without the
# diagnostics that transformed programs may legitimately print on stderr.
function(harness_dump_hash file var)
    file(READ "${file}" content)
    # TpzTiling: non-fatal warning printed (two lines) when a front is small.
    string(REGEX REPLACE "Warning: target volume too small\\.\n\\(TARGET_VOL_L1[^\n]*\n" "" content "${content}")
    string(SHA256 hash "${content}")
    set(${var} "${hash}" PARENT_SCOPE)
endfunction()

# harness_pipeline(<name> <tile> <out-var>): instantiate pipelines/<name>.json.in
# into WORK_DIR; sets <out-var> to the --config arguments (empty for default).
function(harness_pipeline name tile var)
    if(name STREQUAL "default")
        set(${var} "" PARENT_SCOPE)
        return()
    endif()
    set(TILE ${tile})
    configure_file("${PIPELINE_DIR}/${name}.json.in" "${WORK_DIR}/pipeline.json" @ONLY)
    set(${var} --config "${WORK_DIR}/pipeline.json" PARENT_SCOPE)
endfunction()

set(HARNESS_CFLAGS -O2 -g -fopenmp -fno-omit-frame-pointer -ffp-contract=off -w)
if(SANITIZERS)
    list(APPEND HARNESS_CFLAGS -fsanitize=address,undefined -fno-sanitize-recover=all)
endif()
