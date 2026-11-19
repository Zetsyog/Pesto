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

gtest_discover_tests(pesto_gtest PROPERTIES LABELS workflow TIMEOUT 30)

file(GLOB SCOP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.scop")

add_custom_command(
    TARGET pesto_gtest
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${SCOP_FILES} ${TEST_BINARY_DIR}
    COMMENT "Copying .scop files to ${TEST_BINARY_DIR}"
)

add_custom_target(
    unit-tests
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/pesto_gtest
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running unit tests for Pesto"
)

add_dependencies(unit-tests pesto_gtest)
