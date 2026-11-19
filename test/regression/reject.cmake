file(MAKE_DIRECTORY "${WORK_DIR}")
set(ENV{TMPDIR} "${WORK_DIR}")
set(ENV{ASAN_OPTIONS} "detect_leaks=0:halt_on_error=1")
set(ENV{UBSAN_OPTIONS} "halt_on_error=1:print_stacktrace=1")

function(reject name diagnostic)
    file(REMOVE "${WORK_DIR}/generated.c")
    execute_process(COMMAND "${PESTO}" ${ARGN} -o generated.c
        WORKING_DIRECTORY "${WORK_DIR}" RESULT_VARIABLE status
        OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 15)
    file(WRITE "${WORK_DIR}/${name}.log" "${output}\n${errors}")
    if(NOT "${status}" STREQUAL "1" OR NOT errors MATCHES "${diagnostic}"
       OR errors MATCHES "AddressSanitizer|runtime error:" OR EXISTS "${WORK_DIR}/generated.c")
        message(FATAL_ERROR "${name}: expected diagnostic '${diagnostic}' and exit 1, got '${status}'\n${errors}")
    endif()
endfunction()

configure_file("${SOURCE_DIR}/fixtures/pointwise.c" "${WORK_DIR}/input.c" COPYONLY)
foreach(tiles IN ITEMS "[0]" "[-3,2]" "[]")
    file(WRITE "${WORK_DIR}/invalid-tiles.json"
        "{\"passes\":[{\"uuid\":\"pluto\",\"options\":{\"tile\":true,\"tile-sizes\":${tiles}}},{\"uuid\":\"codegen-cloog\"}]}")
    reject("tiles-${tiles}" "Tile sizes must be" --config invalid-tiles.json input.c)
endforeach()

# CLooG supports these domains (a positive regression exercises that path),
# while Pluto's importer cannot represent their existential local dimensions.
configure_file("${SOURCE_DIR}/fixtures/guarded_stride.c" "${WORK_DIR}/stride.c" COPYONLY)
reject(stride "Pluto does not support local dimensions" stride.c)

file(READ "${WORK_DIR}/input.c" source)
string(REPLACE "#pragma endscop"
    "#pragma endscop\n#pragma scop\nfor(i=0;i<n;i++) output[i][0] += 1;\n#pragma endscop" source "${source}")
file(WRITE "${WORK_DIR}/multiple.c" "${source}")
reject(multiple-regions "Only one scop region" multiple.c)

file(GLOB leftovers "${WORK_DIR}/.pesto-*")
if(leftovers)
    message(FATAL_ERROR "Temporary workspace leak: ${leftovers}")
endif()
