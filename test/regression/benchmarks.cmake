# PolyBench/C and NPDP kernels as differential regression tests.
#
# - Allowlisted (kernel, pipeline) pairs at the end of this file are ctest
#   cases labelled "regression;bench", part of check-regressions.
# - check-bench-probe runs EVERY kernel through EVERY pipeline (outside the
#   normal ctest run) and writes bench-probe/probe-report.md: use it to grow
#   the allowlist and to spot allowlisted cases that broke.
option(PESTO_ENABLE_BENCH_REGRESSIONS "Run PolyBench and NPDP kernels as regression tests" ON)
if(NOT PESTO_ENABLE_BENCH_REGRESSIONS)
    return()
endif()

include(FetchContent)
# SOURCE_SUBDIR points nowhere: these are plain source trees, never built.
FetchContent_Declare(pesto_polybench
    URL https://downloads.sourceforge.net/project/polybench/polybench-c-4.2.1-beta.tar.gz
    URL_HASH SHA256=426519ee8443a5f2175de6a3e9328cda8917a5e33053d0e8f59855ab56d689a4
    SOURCE_SUBDIR do-not-build
)
FetchContent_Declare(pesto_npdp
    GIT_REPOSITORY https://github.com/Zetsyog/NPDP_Bench.git
    GIT_TAG a8c2cbf1e194f0fad575324aee6270bc5e628c48
    SOURCE_SUBDIR do-not-build
)
FetchContent_MakeAvailable(pesto_polybench pesto_npdp)
if(NOT EXISTS "${pesto_polybench_SOURCE_DIR}/utilities/polybench.c"
   OR NOT EXISTS "${pesto_npdp_SOURCE_DIR}/include/mem.h")
    message(WARNING "PolyBench/NPDP sources are missing (FETCHCONTENT_FULLY_DISCONNECTED on a fresh "
        "build directory?): benchmark regressions are disabled. Configure once with "
        "-DFETCHCONTENT_FULLY_DISCONNECTED=OFF, or -DPESTO_ENABLE_BENCH_REGRESSIONS=OFF.")
    return()
endif()

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    set(PESTO_BENCH_PYTHON "${Python3_EXECUTABLE}")
else()
    set(PESTO_BENCH_PYTHON python3)
endif()

set(PESTO_BENCH_PROBE_DIR "${CMAKE_BINARY_DIR}/bench-probe")
set(PESTO_BENCH_CONFIG_DIR "${PROJECT_SOURCE_DIR}/utilities/configuration files")

# Kernel classes decide which pipelines apply (see the probe report):
# - algebraic: standard and hybrid algebraic tiling;
# - trapezoidal: only trapezoidal tiling (generic tpz, or the per-kernel
#   configuration; NPDP kernels need its skew), never algebraic tiling;
# - unsupported: not pesto-compatible, only probed through Pluto (pluto
#   pipeline, when PESTO_BENCH_POLYCC is set).
# default/tiled (Pluto scheduling + rectangular tiling) are probed for every
# pesto-compatible kernel. NPDP mea, sw3d and zuker are deliberately ignored.
set(PESTO_POLYBENCH_ALGEBRAIC
    datamining/correlation datamining/covariance
    linear-algebra/kernels/2mm linear-algebra/kernels/3mm linear-algebra/kernels/atax
    linear-algebra/kernels/bicg linear-algebra/kernels/doitgen linear-algebra/kernels/mvt
    linear-algebra/blas/gemm linear-algebra/blas/gemver linear-algebra/blas/gesummv
    linear-algebra/blas/syr2k linear-algebra/blas/syrk linear-algebra/blas/trmm
    linear-algebra/solvers/gramschmidt medley/deriche)
set(PESTO_POLYBENCH_TRAPEZOIDAL
    linear-algebra/solvers/lu linear-algebra/solvers/cholesky medley/nussinov
    stencils/seidel-2d stencils/fdtd-2d linear-algebra/blas/symm stencils/heat-3d
    stencils/jacobi-1d stencils/jacobi-2d linear-algebra/solvers/trisolv)
set(PESTO_POLYBENCH_UNSUPPORTED
    medley/floyd-warshall stencils/adi linear-algebra/solvers/ludcmp linear-algebra/solvers/durbin)
set(PESTO_NPDP_TRAPEZOIDAL
    counting counting_im mcm knuth triang nussinov mcc nw sw)

set(PESTO_BENCH_ALGEBRAIC_PIPELINES default tiled atiling hybrid-2d)
set(PESTO_BENCH_TRAPEZOIDAL_PIPELINES default tiled tpz)
set(PESTO_BENCH_NPDP_PIPELINES default tiled)
set(PESTO_BENCH_POLYCC "" CACHE STRING
    "polycc command for the probe's Pluto reference pipeline (empty: disabled)")

# Extra polycc flags for the Pluto reference pipeline.
set(PESTO_BENCH_POLYCC_FLAGS_heat-3d --nocloogbacktrack)

# _pesto_bench_command(<suite> <kernel> <pipeline> <work-dir> <out-var>)
function(_pesto_bench_command suite kernel pipeline work_dir var)
    get_filename_component(name "${kernel}" NAME)
    if(suite STREQUAL "polybench")
        set(root "${pesto_polybench_SOURCE_DIR}")
        set(config "${PESTO_BENCH_CONFIG_DIR}/polybenchs/${name}.json")
    else()
        set(root "${pesto_npdp_SOURCE_DIR}")
        set(config "${PESTO_BENCH_CONFIG_DIR}/NPDP/${name}.json")
    endif()
    set(${var}
        "${CMAKE_COMMAND}"
        "-DPESTO=$<TARGET_FILE:pesto-cli>" "-DCC=${CMAKE_C_COMPILER}"
        "-DSANITIZERS=${PESTO_REGRESSION_SANITIZERS}"
        "-DSUITE=${suite}" "-DKERNEL=${kernel}" "-DBENCH_ROOT=${root}"
        "-DPIPELINE=${pipeline}" "-DKERNEL_CONFIG=${config}"
        "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}" "-DWORK_DIR=${work_dir}"
        "-DPYTHON=${PESTO_BENCH_PYTHON}" "-DGEN_MAXCUT=${PROJECT_SOURCE_DIR}/utilities/gen_maxcut.py"
        "-DISCC=${BARVINOK_INSTALL_DIR}/bin/iscc" "-DPOLYCC=${PESTO_BENCH_POLYCC}"
        "-DPOLYCC_FLAGS=${PESTO_BENCH_POLYCC_FLAGS_${name}}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/bench.cmake"
        PARENT_SCOPE)
endfunction()

# pesto_bench(<suite> <kernel> PIPELINES <pipeline>...): allowlist entry.
function(pesto_bench suite kernel)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "" "PIPELINES")
    get_filename_component(name "${kernel}" NAME)
    foreach(pipeline IN LISTS arg_PIPELINES)
        set(test bench-${suite}-${name}-${pipeline})
        _pesto_bench_command(${suite} ${kernel} ${pipeline}
            "${PESTO_REGRESSION_ARTIFACT_DIR}/${test}" command)
        add_test(NAME ${test} COMMAND ${command})
        set_tests_properties(${test} PROPERTIES LABELS "regression;bench" TIMEOUT 900 PROCESSORS 2)
        set_property(DIRECTORY APPEND PROPERTY PESTO_BENCH_ALLOWLIST "${suite}/${name}/${pipeline}")
    endforeach()
endfunction()

# The probe lives in its own ctest directory, so plain `ctest` never runs it.
function(_pesto_bench_probe)
    set(content "# Generated by test/regression/benchmarks.cmake\n")
    list(JOIN PESTO_BENCH_ALGEBRAIC_PIPELINES "," algebraic)
    list(JOIN PESTO_BENCH_TRAPEZOIDAL_PIPELINES "," trapezoidal)
    list(JOIN PESTO_BENCH_NPDP_PIPELINES "," npdp)
    set(cases)
    foreach(kernel IN LISTS PESTO_POLYBENCH_ALGEBRAIC)
        list(APPEND cases "polybench|${kernel}|${algebraic}|")
    endforeach()
    foreach(kernel IN LISTS PESTO_POLYBENCH_TRAPEZOIDAL)
        list(APPEND cases "polybench|${kernel}|${trapezoidal}|kernel")
    endforeach()
    foreach(kernel IN LISTS PESTO_POLYBENCH_UNSUPPORTED)
        list(APPEND cases "polybench|${kernel}||")
    endforeach()
    foreach(kernel IN LISTS PESTO_NPDP_TRAPEZOIDAL)
        list(APPEND cases "npdp|${kernel}|${npdp}|kernel")
    endforeach()
    foreach(case IN LISTS cases)
        string(REPLACE "|" ";" case "${case}")
        list(GET case 0 suite)
        list(GET case 1 kernel)
        list(GET case 2 pipelines)
        list(GET case 3 trapezoidal_config)
        string(REPLACE "," ";" pipelines "${pipelines}")
        get_filename_component(name "${kernel}" NAME)
        if(suite STREQUAL "polybench")
            set(config_dir "${PESTO_BENCH_CONFIG_DIR}/polybenchs")
        else()
            set(config_dir "${PESTO_BENCH_CONFIG_DIR}/NPDP")
        endif()
        if(trapezoidal_config AND EXISTS "${config_dir}/${name}.json")
            list(APPEND pipelines kernel)
        endif()
        if(PESTO_BENCH_POLYCC)
            list(APPEND pipelines pluto)
        endif()
        foreach(pipeline IN LISTS pipelines)
            set(test probe-${suite}-${name}-${pipeline})
            _pesto_bench_command(${suite} ${kernel} ${pipeline}
                "${PESTO_BENCH_PROBE_DIR}/cases/${suite}/${name}/${pipeline}" command)
            # Quoted arguments: CMake lists mishandle bracket arguments.
            set(line "add_test(${test}")
            foreach(arg IN LISTS command)
                string(REPLACE "\\" "\\\\" arg "${arg}")
                string(REPLACE "\"" "\\\"" arg "${arg}")
                string(APPEND line " \"${arg}\"")
            endforeach()
            string(APPEND content "${line})\n")
            string(APPEND content "set_tests_properties(${test} PROPERTIES TIMEOUT 900 LABELS probe)\n")
        endforeach()
    endforeach()
    file(GENERATE OUTPUT "${PESTO_BENCH_PROBE_DIR}/CTestTestfile.cmake" CONTENT "${content}")
endfunction()
_pesto_bench_probe()

cmake_host_system_information(RESULT PESTO_BENCH_PROBE_JOBS QUERY NUMBER_OF_LOGICAL_CORES)
add_custom_target(check-bench-probe
    COMMAND ${CMAKE_COMMAND}
        -DCTEST=${CMAKE_CTEST_COMMAND} -DPROBE_DIR=${PESTO_BENCH_PROBE_DIR}
        -DJOBS=${PESTO_BENCH_PROBE_JOBS}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/probe_report.cmake
    DEPENDS pesto-cli
    USES_TERMINAL
    COMMENT "Probing every PolyBench/NPDP kernel through every pipeline")

# --- Allowlist ---------------------------------------------------------------
# Chosen from the probe report (2026-11-26). Leave a reason next to every
# excluded pair; promote it once the probe shows it passing.

# PolyBench, algebraic class.
foreach(kernel IN ITEMS
        datamining/correlation datamining/covariance
        linear-algebra/kernels/atax linear-algebra/kernels/doitgen
        linear-algebra/kernels/mvt linear-algebra/blas/gemm linear-algebra/blas/gemver
        linear-algebra/blas/gesummv linear-algebra/blas/syr2k linear-algebra/blas/syrk
        linear-algebra/blas/trmm medley/deriche)
    pesto_bench(polybench ${kernel} PIPELINES default tiled atiling hybrid-2d)
endforeach()
pesto_bench(polybench linear-algebra/solvers/gramschmidt PIPELINES default tiled hybrid-2d)
# Excluded: bicg/default (the i loop is marked parallel while s[j] accumulates
# over i: intermittent wrong result with 4 threads).
pesto_bench(polybench linear-algebra/kernels/bicg PIPELINES tiled atiling hybrid-2d)
# Excluded: gramschmidt/atiling (pesto aborts); 2mm/atiling, 3mm/atiling
# (pesto aborts); 2mm/hybrid-2d (runtime crash in the generated code);
# 3mm/hybrid-2d (wrong result).
pesto_bench(polybench linear-algebra/kernels/2mm PIPELINES default tiled)
pesto_bench(polybench linear-algebra/kernels/3mm PIPELINES default tiled)

# PolyBench, trapezoidal class.
foreach(kernel IN ITEMS stencils/fdtd-2d stencils/heat-3d stencils/jacobi-1d stencils/jacobi-2d)
    pesto_bench(polybench ${kernel} PIPELINES default tiled tpz)
endforeach()
pesto_bench(polybench stencils/seidel-2d PIPELINES default tiled tpz kernel)
pesto_bench(polybench linear-algebra/blas/symm PIPELINES default tiled tpz kernel)
# Excluded: lu tpz/kernel (wrong result with 2+ threads); cholesky tpz/kernel
# (wrong result, even with 1 thread); nussinov/tpz (pesto aborts) and
# nussinov/kernel (wrong result with 4 threads, Pluto itself is wrong on
# nussinov too); trisolv/tpz (pesto aborts) and trisolv/kernel (skew: 'j' is
# not an iterator of every statement of the upstream source).
foreach(kernel IN ITEMS
        linear-algebra/solvers/lu linear-algebra/solvers/cholesky
        medley/nussinov linear-algebra/solvers/trisolv)
    pesto_bench(polybench ${kernel} PIPELINES default tiled)
endforeach()

# NPDP, trapezoidal class (the "kernel" configuration is the trapezoidal one).
pesto_bench(npdp knuth PIPELINES default tiled kernel)
pesto_bench(npdp nussinov PIPELINES default tiled kernel)
pesto_bench(npdp counting PIPELINES default tiled)
pesto_bench(npdp counting_im PIPELINES default tiled)
pesto_bench(npdp mcc PIPELINES default tiled)
foreach(kernel IN ITEMS mcm nw sw triang)
    pesto_bench(npdp ${kernel} PIPELINES tiled)
endforeach()
# Excluded: mcm, nw, sw, triang default (wrong result with 4 threads; Pluto's
# own polycc output is correct); counting/kernel (pesto's context assumes
# l >= 10, the kernel runs with l = 0); mcc/kernel (tpz finds no loop to tile
# and still exits 0; the skewed output is wrong); counting_im, mcm, nw, sw,
# triang kernel (wrong result, mostly with 2+ threads).
# -----------------------------------------------------------------------------
get_property(allowlist DIRECTORY PROPERTY PESTO_BENCH_ALLOWLIST)
list(JOIN allowlist "\n" allowlist)
file(WRITE "${PESTO_BENCH_PROBE_DIR}/allowlist.txt" "${allowlist}\n")
