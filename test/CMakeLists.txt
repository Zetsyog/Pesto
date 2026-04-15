# unit tests (Google Test)
add_subdirectory(GTest)

# Polybench tests
set(PESTO_TEST_POLYBENCH_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/test_polybench.sh")
set(PESTO_TEST_POLYBENCH_ALGEBRAIC_BIN "${CMAKE_CURRENT_BINARY_DIR}/test_polybench_algebraic.sh")
set(PESTO_TEST_POLYBENCH_HYBRID_BIN "${CMAKE_CURRENT_BINARY_DIR}/test_polybench_hybrid.sh")
set(PESTO_TEST_POLYBENCH_DYNSCHEDULE_BIN "${CMAKE_CURRENT_BINARY_DIR}/test_polybench_dynschedule.sh")
set(PESTO_TEST_POLYBENCH_TPZ_BIN "${CMAKE_CURRENT_BINARY_DIR}/test_polybench_tpz.sh")

set(PESTO_TEST_POLYBENCH_DATASET
    "LARGE_DATASET"
    CACHE STRING "Specify dataset size for polybench tests"
)
set(PESTO_TEST_POLYBENCH_TILE_DIVIDER_MIN
    2
    CACHE STRING "Specify minimum tile divider size for polybench tests"
)
set(PESTO_TEST_POLYBENCH_TILE_DIVIDER_MAX
    32
    CACHE STRING "Specify maximum tile divider size for polybench tests"
)

message(STATUS "Polybench tests with dataset: ${PESTO_TEST_POLYBENCH_DATASET}")
message(
    STATUS
    "Polybench tests with tile divider range: ${PESTO_TEST_POLYBENCH_TILE_DIVIDER_MIN} - ${PESTO_TEST_POLYBENCH_TILE_DIVIDER_MAX}"
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_polybench.sh.in
    ${PESTO_TEST_POLYBENCH_SCRIPT}
    FILE_PERMISSIONS
    OWNER_READ
    OWNER_WRITE
    GROUP_READ
    WORLD_READ
    @ONLY
)

foreach(test_bin IN ITEMS
    ${PESTO_TEST_POLYBENCH_ALGEBRAIC_BIN}
    ${PESTO_TEST_POLYBENCH_HYBRID_BIN}
    ${PESTO_TEST_POLYBENCH_DYNSCHEDULE_BIN}
    ${PESTO_TEST_POLYBENCH_TPZ_BIN}
)
    get_filename_component(input_script ${test_bin} NAME)
    get_filename_component(basename ${test_bin} NAME_WE)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/${input_script}.in
        ${test_bin}
        FILE_PERMISSIONS
        OWNER_READ
        OWNER_WRITE
        OWNER_EXECUTE
        GROUP_READ
        GROUP_EXECUTE
        WORLD_READ
        WORLD_EXECUTE
        @ONLY
    )

    string(REPLACE "_" "-" target-name ${basename})
    add_custom_target(
        ${target-name}
        COMMAND bash ${test_bin}
        DEPENDS pesto-cli
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/polybench-4.2.1/
    )
endforeach()

