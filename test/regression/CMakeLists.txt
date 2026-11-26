option(PESTO_REGRESSION_SANITIZERS "Check baseline and generated C with ASan and UBSan" ON)
set(PESTO_REGRESSION_ARTIFACT_DIR "${CMAKE_CURRENT_BINARY_DIR}/cases" CACHE PATH
    "Directory for compiler regression sources, commands, diagnostics, and dumps")

if(PESTO_REGRESSION_SANITIZERS)
    include(CheckCCompilerFlag)
    include(CMakePushCheckState)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_LINK_OPTIONS -fsanitize=address,undefined)
    check_c_compiler_flag("-fsanitize=address,undefined" PESTO_C_HAS_SANITIZERS)
    cmake_pop_check_state()
    if(NOT PESTO_C_HAS_SANITIZERS)
        message(FATAL_ERROR "Regression sanitizers require ASan/UBSan support; disable with -DPESTO_REGRESSION_SANITIZERS=OFF")
    endif()
endif()

# pesto_regression(<fixture> <pipeline> [TILE <size>]): TILE fills @TILE@ in
# pipelines with fixed Pluto tile sizes. Parameter-tiling pipelines (algebraic,
# hybrid) take none: their tile sizes are the DIVn macros of the generated code,
# set at compile time by the harness.
function(pesto_regression fixture pipeline)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "TILE" "")
    set(name regression-${fixture}-${pipeline})
    set(tile_args)
    if(DEFINED arg_TILE)
        string(APPEND name -${arg_TILE})
        set(tile_args --tile ${arg_TILE})
    endif()
    add_test(NAME ${name}
        COMMAND ${PESTO_TEST_PYTHON} ${PESTO_TEST_HARNESS_DIR}/run_fixture.py
            --pesto $<TARGET_FILE:pesto-cli> --cc ${CMAKE_C_COMPILER}
            --fixture ${fixture} --pipeline ${pipeline} ${tile_args}
            --sanitizers ${PESTO_REGRESSION_SANITIZERS}
            --source-dir ${CMAKE_CURRENT_SOURCE_DIR}
            --work-dir ${PESTO_REGRESSION_ARTIFACT_DIR}/${name})
    set_tests_properties(${name} PROPERTIES LABELS regression TIMEOUT 120 PROCESSORS 4)
endfunction()

foreach(fixture IN ITEMS pointwise triangle scalar reduction multi_statement stencil guarded negative_bounds)
    pesto_regression(${fixture} default)
    pesto_regression(${fixture} pluto-like TILE 3)
    pesto_regression(${fixture} pluto-like TILE 64)
endforeach()
foreach(fixture IN ITEMS pointwise triangle)
    pesto_regression(${fixture} hybrid)
    pesto_regression(${fixture} algebraic)
endforeach()
pesto_regression(guarded_stride codegen)

add_test(NAME regression-rejections
    COMMAND ${PESTO_TEST_PYTHON} ${PESTO_TEST_HARNESS_DIR}/reject.py --pesto $<TARGET_FILE:pesto-cli>
        --source-dir ${CMAKE_CURRENT_SOURCE_DIR}
        --work-dir ${PESTO_REGRESSION_ARTIFACT_DIR}/regression-rejections)
set_tests_properties(regression-rejections PROPERTIES LABELS regression TIMEOUT 90)

add_custom_target(check-regressions
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --parallel 8 -L "workflow|regression"
    DEPENDS pesto-cli pesto_gtest WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    VERBATIM
    COMMENT "Running fast compiler correctness regressions")

include(${CMAKE_CURRENT_SOURCE_DIR}/benchmarks.cmake)
