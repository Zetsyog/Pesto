# Test scripts (test/harness/*.py) are plain-stdlib Python, run with -I so the
# environment cannot inject modules, and -B so they never write into the source tree.
find_package(Python3 3.8 COMPONENTS Interpreter REQUIRED)
set(PESTO_TEST_PYTHON "${Python3_EXECUTABLE}" -I -B)
set(PESTO_TEST_HARNESS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/harness")

# unit tests (Google Test)
add_subdirectory(GTest)
add_subdirectory(regression)

# Fast correctness checks, independent of the external benchmark suites.
foreach(workflow_case IN ITEMS default json skew errors concurrent)
    add_test(
        NAME workflow-${workflow_case}
        COMMAND ${PESTO_TEST_PYTHON} ${PESTO_TEST_HARNESS_DIR}/workflow.py
            --pesto $<TARGET_FILE:pesto-cli>
            --cc ${CMAKE_C_COMPILER}
            --case ${workflow_case}
            --source-dir ${CMAKE_CURRENT_SOURCE_DIR}/workflow
            --work-dir ${CMAKE_CURRENT_BINARY_DIR}/workflow-${workflow_case}
    )
    set_tests_properties(workflow-${workflow_case} PROPERTIES LABELS workflow TIMEOUT 120)
endforeach()

add_custom_target(
    check-workflow
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L workflow
    DEPENDS pesto-cli pesto_gtest
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Checking transform, compile, and result comparison workflows"
)

# Performance benchmark repository (pesto-bench). Correctness on PolyBench and
# NPDP is covered by test/regression/benchmarks.cmake, which fetches its own
# pinned sources; this checkout only serves the performance scripts.
option(PESTO_ENABLE_BENCHMARKS "Fetch the pesto-bench performance benchmark repository" ON)
if(NOT PESTO_ENABLE_BENCHMARKS)
    return()
endif()

message(STATUS "Fetching pesto-bench repository")
fetchcontent_populate(
    pesto-bench
    GIT_REPOSITORY https://github.com/Zetsyog/pesto-bench.git #
    GIT_TAG 0b92b2d84c5faa8aa0e17c66cbc2e22768342e84
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/pesto-bench
)
