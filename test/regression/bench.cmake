# Differential check of one PolyBench or NPDP kernel through one pipeline.
#
# Inputs: SUITE (polybench|npdp), KERNEL (path inside BENCH_ROOT), BENCH_ROOT,
#         PIPELINE (a pipelines/*.json.in name, "default", or "kernel" with
#         KERNEL_CONFIG, or "pluto" with POLYCC [POLYCC_FLAGS]), PESTO, CC, SANITIZERS, SOURCE_DIR, WORK_DIR,
#         PYTHON, GEN_MAXCUT, ISCC.
#
# The original and the transformed kernel are compiled with the same flags and
# run on several problem sizes; the transformed one with several tile divider
# sets and thread counts. Result dumps (stderr) must hash identically.
include("${CMAKE_CURRENT_LIST_DIR}/harness.cmake")
set(PIPELINE_DIR "${SOURCE_DIR}/pipelines")
get_filename_component(name "${KERNEL}" NAME)
set(CASE_NAME "${SUITE}/${KERNEL}/${PIPELINE}")

# Fresh copy of the kernel sources: generated headers from an earlier run must
# not hide one that this run forgot to emit.
file(GLOB stale "${WORK_DIR}/*.c" "${WORK_DIR}/*.h" "${WORK_DIR}/*.err" "${WORK_DIR}/*.out")
if(stale)
    file(REMOVE ${stale})
endif()
file(GLOB kernel_sources "${BENCH_ROOT}/${KERNEL}/*.c" "${BENCH_ROOT}/${KERNEL}/*.h")
file(COPY ${kernel_sources} DESTINATION "${WORK_DIR}")

if(PIPELINE STREQUAL "pluto")
    # Reference run: the same kernel through Pluto's polycc, to tell pesto
    # bugs from kernels that the polyhedral toolchain does not handle.
    if(NOT POLYCC)
        harness_fail(transform-fail "pluto pipeline needs POLYCC")
    endif()
    separate_arguments(polycc NATIVE_COMMAND "${POLYCC}")
    # --nounroll (as in pesto-bench): unrolled output takes minutes to build
    # with sanitizers.
    separate_arguments(polycc_flags NATIVE_COMMAND "${POLYCC_FLAGS}")
    harness_run(transform transform-fail ${polycc} ${name}.c --tile --parallel --nounroll ${polycc_flags} -o generated.c)
else()
    if(PIPELINE STREQUAL "kernel")
        configure_file("${KERNEL_CONFIG}" "${WORK_DIR}/pipeline.json" COPYONLY)
        set(config_args --config "${WORK_DIR}/pipeline.json")
    else()
        harness_pipeline(${PIPELINE} 7 config_args)
    endif()
    harness_run(transform transform-fail "${PESTO}" ${config_args} ${name}.c -o generated.c)
endif()
if(NOT EXISTS "${WORK_DIR}/generated.c")
    harness_fail(transform-fail "no generated source")
endif()
file(READ "${WORK_DIR}/generated.c" generated)
set(param_tiled FALSE)
if(generated MATCHES "DIV0")
    set(param_tiled TRUE)
endif()

# TpzTiling leaves the max_cut_volume() helper to gen_maxcut.py, documented
# by a comment in the output. Extract that command once.
set(tpz FALSE)
if(generated MATCHES "#include \"maxcut.h\"")
    set(tpz TRUE)
    if(NOT generated MATCHES "--domain \"([^\"]*)\"")
        harness_fail(maxcut-fail "no gen_maxcut domain in the generated source")
    endif()
    set(maxcut_domain "${CMAKE_MATCH_1}")
    set(maxcut_stride 1)
    if(generated MATCHES "--t \\$\\(\\(DIV0 \\* ([0-9]+)\\)\\)")
        set(maxcut_stride ${CMAKE_MATCH_1})
    endif()
    set(maxcut_f1)
    if(generated MATCHES "--f1 (-?[0-9]+)")
        set(maxcut_f1 --f1 ${CMAKE_MATCH_1})
    endif()
    set(maxcut_params)
    string(FIND "${maxcut_domain}" "->" arrow)
    if(maxcut_domain MATCHES "^\\[" AND arrow GREATER 0)
        string(SUBSTRING "${maxcut_domain}" 0 ${arrow} head)
        string(REGEX REPLACE "^\\[(.*)\\] *$" "\\1" head "${head}")
        string(REPLACE "," ";" maxcut_params "${head}")
        list(TRANSFORM maxcut_params STRIP)
    endif()
endif()

# Problem instances: <tag>|<defines separated by ,>
if(SUITE STREQUAL "polybench")
    set(variants "mini|-DMINI_DATASET" "small|-DSMALL_DATASET")
    set(suite_flags -I "${BENCH_ROOT}/utilities" -I "${WORK_DIR}" -DPOLYBENCH_DUMP_ARRAYS)
    set(suite_sources "${BENCH_ROOT}/utilities/polybench.c")
elseif(SUITE STREQUAL "npdp")
    set(variants)
    # The transformed code assumes the parameters are at least 10 (pesto's
    # default context), so smaller sizes are out of contract.
    foreach(n IN ITEMS 10 23 64)
        foreach(seed IN ITEMS 1 7)
            list(APPEND variants "n${n}-s${seed}|-DN=${n},-DNPDP_RSEED=${seed}")
        endforeach()
    endforeach()
    set(suite_flags -I "${BENCH_ROOT}/include" -I "${WORK_DIR}" -DNPDP_DUMP)
    set(suite_sources)
else()
    harness_fail(baseline-fail "unknown suite ${SUITE}")
endif()

set(dividers "standard|")
if(param_tiled)
    set(dividers
        "unit|-DDIV0=1,-DDIV1=1,-DDIV2=1,-DDIV3=1"
        "uneven|-DDIV0=3,-DDIV1=5,-DDIV2=7,-DDIV3=11"
        "oversized|-DDIV0=64,-DDIV1=65,-DDIV2=66,-DDIV3=67")
endif()
if(tpz)
    # TpzTiling: DIV0 is the band (gap) width, DIV2 the inner tile size; DIV1
    # keeps its generated default. Ranges follow the pesto-bench TPZ runs
    # (DIV0 >= 2, DIV2 >= 4).
    set(dividers
        "small|-DDIV0=2,-DDIV2=4"
        "uneven|-DDIV0=3,-DDIV2=5"
        "oversized|-DDIV0=64,-DDIV2=67")
endif()

# write_maxcut(<variant defines> <DIV0>): maxcut.h for this problem instance.
# gen_maxcut.py keeps the domain parameters as arguments, while the generated
# code calls max_cut_volume(b): bind them to this instance's values.
function(write_maxcut tag defines div0)
    set(values)
    if(NOT "${maxcut_params}" STREQUAL "")
        if(SUITE STREQUAL "npdp")
            foreach(p IN LISTS maxcut_params)
                if(p STREQUAL "n")
                    string(REGEX MATCH "-DN=([0-9]+)" _ "${defines}")
                    list(APPEND values ${CMAKE_MATCH_1})
                elseif(p STREQUAL "l")
                    list(APPEND values 0) # NPDP_MIN_LOOP default
                else()
                    harness_fail(maxcut-fail "unknown NPDP parameter '${p}'")
                endif()
            endforeach()
        else()
            set(src "#include <stdio.h>\n#include \"polybench.h\"\n#include \"${name}.h\"\nint main(void) {\n")
            # _PB_X expands to the scalar x (POLYBENCH_LOOP_BOUND); its value
            # is the dataset macro X.
            foreach(p IN LISTS maxcut_params)
                string(REGEX REPLACE "^_PB_" "" macro "${p}")
                string(APPEND src "  printf(\"%ld;\", (long)(${macro}));\n")
            endforeach()
            string(APPEND src "  return 0;\n}\n")
            file(WRITE "${WORK_DIR}/params-${tag}.c" "${src}")
            harness_run(params-${tag}-build maxcut-fail "${CC}" ${suite_flags} ${defines} params-${tag}.c -o params-${tag})
            harness_run(params-${tag} maxcut-fail "${WORK_DIR}/params-${tag}")
            file(READ "${WORK_DIR}/params-${tag}.out" values)
            string(REGEX REPLACE ";$" "" values "${values}")
        endif()
    endif()
    # Pinning the parameters to this instance also keeps lexmin(x1) from being
    # piecewise in a parameter (e.g. NPDP l), which gen_maxcut cannot use.
    set(domain "${maxcut_domain}")
    set(pins "")
    foreach(p v IN ZIP_LISTS maxcut_params values)
        string(APPEND pins "${p} = ${v} and ")
    endforeach()
    if(NOT pins STREQUAL "")
        string(REGEX REPLACE "^([^{]*\\{[^:]*:)" "\\1 ${pins}" domain "${domain}")
    endif()
    math(EXPR t "${div0} * ${maxcut_stride}")
    harness_run(maxcut-${tag}-t${t} maxcut-fail "${PYTHON}" "${GEN_MAXCUT}"
        --domain "${domain}" --t ${t} ${maxcut_f1}
        --func pesto_maxcut_impl --iscc "${ISCC}" --no-backup --out maxcut.h)
    string(REPLACE ";" ", " args "${values}")
    if(NOT "${args}" STREQUAL "")
        string(APPEND args ", ")
    endif()
    file(APPEND "${WORK_DIR}/maxcut.h"
        "\nstatic long max_cut_volume(long b) { return pesto_maxcut_impl(${args}b); }\n")
endfunction()

foreach(variant IN LISTS variants)
    string(REPLACE "|" ";" parts "${variant}")
    list(GET parts 0 tag)
    list(GET parts 1 defines)
    string(REPLACE "," ";" defines "${defines}")

    harness_run(compile-baseline-${tag} baseline-fail "${CC}" ${HARNESS_CFLAGS} ${suite_flags} ${defines}
        ${suite_sources} ${name}.c -o baseline-${tag} -lm)
    set(ENV{OMP_NUM_THREADS} 1)
    harness_run(baseline-${tag} baseline-fail "${WORK_DIR}/baseline-${tag}")
    file(SIZE "${WORK_DIR}/baseline-${tag}.err" dump_size)
    if(dump_size EQUAL 0)
        harness_fail(baseline-fail "baseline produced no result dump (${tag})")
    endif()
    harness_dump_hash("${WORK_DIR}/baseline-${tag}.err" expected)

    foreach(divider IN LISTS dividers)
        string(REPLACE "|" ";" dparts "${divider}")
        list(GET dparts 0 dtag)
        set(tile_flags)
        list(LENGTH dparts dlen)
        if(dlen GREATER 1)
            list(GET dparts 1 tile_flags)
            string(REPLACE "," ";" tile_flags "${tile_flags}")
        endif()
        set(run "${tag}-${dtag}")
        if(tpz)
            set(div0 1)
            if(tile_flags MATCHES "-DDIV0=([0-9]+)")
                set(div0 ${CMAKE_MATCH_1})
            elseif(generated MATCHES "#define DIV0 ([0-9]+)")
                set(div0 ${CMAKE_MATCH_1})
            endif()
            write_maxcut(${tag} "${defines}" ${div0})
        endif()
        # maxcut.h can reach megabytes for large DIV0: allow a slow build.
        set(PHASE_TIMEOUT 300)
        harness_run(compile-${run} compile-fail "${CC}" ${HARNESS_CFLAGS} ${suite_flags} ${defines} ${tile_flags}
            ${suite_sources} generated.c -o transformed-${run} -lm)
        set(PHASE_TIMEOUT 60)
        foreach(threads IN ITEMS 1 2 4)
            set(ENV{OMP_NUM_THREADS} ${threads})
            harness_run(${run}-t${threads} crash "${WORK_DIR}/transformed-${run}")
            harness_dump_hash("${WORK_DIR}/${run}-t${threads}.err" actual)
            if(NOT actual STREQUAL expected)
                harness_fail(mismatch "${run}, threads=${threads}\nExpected: ${WORK_DIR}/baseline-${tag}.err\nActual: ${WORK_DIR}/${run}-t${threads}.err")
            endif()
        endforeach()
        file(REMOVE "${WORK_DIR}/transformed-${run}")
    endforeach()
endforeach()

if(PIPELINE MATCHES "^(atiling|hybrid-2d)$" AND NOT param_tiled)
    harness_pass(pass-untiled)
else()
    harness_pass()
endif()
