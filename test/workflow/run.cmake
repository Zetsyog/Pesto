# Exercise the public CLI, the generated C compiler, and the resulting program.
file(MAKE_DIRECTORY "${WORK_DIR}")
set(ENV{TMPDIR} "${WORK_DIR}")
set(ENV{ASAN_OPTIONS} "detect_leaks=0:halt_on_error=1")
set(ENV{UBSAN_OPTIONS} "halt_on_error=1:print_stacktrace=1")

function(run)
    execute_process(
        COMMAND ${ARGV}
        WORKING_DIRECTORY "${WORK_DIR}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors
        TIMEOUT 30
    )
    if(NOT "${status}" STREQUAL "0")
        message(FATAL_ERROR "Command failed (${status}): ${ARGV}\n${output}\n${errors}")
    endif()
endfunction()

function(reject)
    execute_process(
        COMMAND "${PESTO}" ${ARGV}
        WORKING_DIRECTORY "${WORK_DIR}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors
        TIMEOUT 30
    )
    # Signals and timeouts must fail the test, rather than count as rejection.
    if(NOT "${status}" STREQUAL "1")
        message(FATAL_ERROR "Expected exit 1, got '${status}': ${ARGV}\n${output}\n${errors}")
    endif()
endfunction()

function(check_cleanup)
    file(GLOB leftovers "${WORK_DIR}/.pesto-*")
    if(leftovers)
        message(FATAL_ERROR "Temporary directories were not removed: ${leftovers}")
    endif()
endfunction()

configure_file("${SOURCE_DIR}/kernel.c" "${WORK_DIR}/input.c" COPYONLY)
file(MAKE_DIRECTORY "${WORK_DIR}/output with spaces")
set(generated "${WORK_DIR}/output with spaces/generated.c")
set(output_args -o "${generated}")
if(CASE STREQUAL "default")
    set(generated "${WORK_DIR}/input.pesto.c")
    set(output_args)
endif()
file(REMOVE "${generated}")

if(CASE STREQUAL "errors")
    run("${PESTO}" --help)
    run("${PESTO}" --version)
    reject()
    reject(--unknown input.c)
    reject(-o)
    reject(input.c extra.c)
    reject(missing.c)
    reject(input.c -o missing-directory/output.c)
    reject(input.c -o input.c)
    file(MAKE_DIRECTORY "${WORK_DIR}/destination-directory")
    file(WRITE "${WORK_DIR}/destination-directory/sentinel" "keep this directory")
    reject(input.c -o destination-directory)
    if(NOT EXISTS "${WORK_DIR}/destination-directory/sentinel")
        message(FATAL_ERROR "Failed publication changed the destination directory")
    endif()
    file(READ "${WORK_DIR}/input.c" actual)
    file(READ "${SOURCE_DIR}/kernel.c" expected)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Failed invocation changed its input")
    endif()
    file(WRITE "${WORK_DIR}/bad.json" "{broken")
    reject(--config bad.json input.c -o "${generated}")
    file(WRITE "${WORK_DIR}/bad.json" "{\"passes\":[{}]}")
    reject(--config bad.json input.c -o "${generated}")
    file(WRITE "${WORK_DIR}/bad.json" "{\"passes\":[{\"uuid\":\"unknown\"}]}")
    reject(--config bad.json input.c -o "${generated}")
    file(WRITE "${WORK_DIR}/bad.json" "{\"passes\":{}}")
    reject(--config bad.json input.c -o "${generated}")
    # An incompatible pass before code generation must be rejected, too.
    file(WRITE "${WORK_DIR}/bad.json"
        "{\"passes\":[{\"uuid\":\"codegen-clast\"},{\"uuid\":\"pluto\"}]}")
    reject(--config bad.json input.c -o "${generated}")
    file(WRITE "${WORK_DIR}/sentinel" "keep this output")
    file(WRITE "${WORK_DIR}/bad.json" "{\"passes\":[{}]}")
    reject(--config bad.json input.c -o sentinel)
    # Fail the external preprocessor after code generation has succeeded.
    file(MAKE_DIRECTORY "${WORK_DIR}/fake-bin")
    file(WRITE "${WORK_DIR}/fake-bin/gcc" "#!/bin/sh\nexit 42\n")
    file(CHMOD "${WORK_DIR}/fake-bin/gcc" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
    set(saved_path "$ENV{PATH}")
    set(ENV{PATH} "${WORK_DIR}/fake-bin:$ENV{PATH}")
    reject(input.c -o sentinel)
    set(ENV{PATH} "${saved_path}")
    file(READ "${WORK_DIR}/sentinel" actual)
    if(NOT actual STREQUAL "keep this output")
        message(FATAL_ERROR "Failed invocation changed existing output")
    endif()
    check_cleanup()
    return()
endif()

if(CASE STREQUAL "concurrent")
    run(bash "${SOURCE_DIR}/concurrent.sh" "${PESTO}" "${CC}")
    check_cleanup()
    return()
endif()

set(config_args)
if(CASE STREQUAL "json")
    list(APPEND config_args --config "${SOURCE_DIR}/default.json")
elseif(CASE STREQUAL "skew")
    file(WRITE "${WORK_DIR}/skew.json"
        "{\"passes\":[{\"uuid\":\"skew\",\"transform\":[\"x = i\",\"y = j\"]},{\"uuid\":\"codegen-cloog\"}]}")
    list(APPEND config_args --config "${WORK_DIR}/skew.json")
endif()
run("${PESTO}" ${config_args} input.c ${output_args})
if(NOT EXISTS "${generated}")
    message(FATAL_ERROR "PESTO reported success without writing output")
endif()

foreach(size IN ITEMS 1 7 24)
    run("${CC}" -O2 -fopenmp "-DSIZE=${size}" input.c -o baseline -lm)
    run("${CC}" -O2 -fopenmp "-DSIZE=${size}" "${generated}" -o transformed -lm)
    execute_process(COMMAND "${WORK_DIR}/baseline" OUTPUT_VARIABLE expected RESULT_VARIABLE status TIMEOUT 10)
    if(NOT "${status}" STREQUAL "0")
        message(FATAL_ERROR "Baseline failed: ${status}")
    endif()
    foreach(threads IN ITEMS 1 2)
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E env "OMP_NUM_THREADS=${threads}" "${WORK_DIR}/transformed"
            OUTPUT_VARIABLE actual ERROR_VARIABLE errors RESULT_VARIABLE status TIMEOUT 10
        )
        if(NOT "${status}" STREQUAL "0" OR NOT actual STREQUAL expected)
            message(FATAL_ERROR "Generated output differs: SIZE=${size}, threads=${threads}, status=${status}\n${errors}")
        endif()
    endforeach()
endforeach()
check_cleanup()
