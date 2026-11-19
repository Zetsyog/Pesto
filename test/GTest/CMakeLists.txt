enable_testing()
include(GoogleTest)

set(TEST_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_executable(
    pesto_gtest
    workflow_tests.cpp
    ${PROJECT_SOURCE_DIR}/cli/src/Options.cpp
)

add_dependencies(pesto_gtest pesto)

target_link_libraries(pesto_gtest pesto GTest::gtest_main)
target_include_directories(pesto_gtest PRIVATE ${PROJECT_SOURCE_DIR}/cli/include)
target_compile_definitions(pesto_gtest PRIVATE _GLIBCXX_ASSERTIONS)

# These tests have no runtime-generated parameters. Scan their declarations so
# discovery never executes an instrumented binary as part of the link step.
gtest_add_tests(TARGET pesto_gtest SOURCES workflow_tests.cpp TEST_LIST PESTO_UNIT_TESTS)
set_tests_properties(${PESTO_UNIT_TESTS} PROPERTIES LABELS workflow TIMEOUT 30
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:halt_on_error=1")

file(GLOB SCOP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.scop")

add_custom_command(
    TARGET pesto_gtest
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${SCOP_FILES} ${TEST_BINARY_DIR}
    COMMENT "Copying .scop files to ${TEST_BINARY_DIR}"
)

add_custom_target(
    unit-tests
    COMMAND ${CMAKE_COMMAND} -E env ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 $<TARGET_FILE:pesto_gtest>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running unit tests for Pesto"
)

add_dependencies(unit-tests pesto_gtest)
