enable_testing()

set(TEST_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_executable(
    Pesto_test
    # osl_to_isl_test.cpp
    dumb_test.cpp
)

add_dependencies(Pesto_test pesto)

target_link_libraries(
    Pesto_test
    pesto
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(Pesto_test)

file(GLOB SCOP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.scop")

add_custom_command(
    TARGET Pesto_test PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${SCOP_FILES} ${TEST_BINARY_DIR}
    COMMENT "Copying .scop files to ${TEST_BINARY_DIR}"
)

add_custom_target(Pesto_check
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/Pesto_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running unit tests for Pesto"
)

add_dependencies(Pesto_check Pesto_test)