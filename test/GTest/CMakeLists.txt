enable_testing()
include(GoogleTest)

set(TEST_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_executable(
	pesto_gtest
	# osl_to_isl_test.cpp
	dumb_test.cpp
)

add_dependencies(pesto_gtest pesto)

target_link_libraries(pesto_gtest pesto GTest::gtest_main)

gtest_discover_tests(pesto_gtest)

file(GLOB SCOP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.scop")

add_custom_command(
	TARGET pesto_gtest
	PRE_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy ${SCOP_FILES} ${TEST_BINARY_DIR}
	COMMENT "Copying .scop files to ${TEST_BINARY_DIR}"
)

add_custom_target(
	pesto-unit-tests
	COMMAND ${CMAKE_CURRENT_BINARY_DIR}/pesto_gtest
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMENT "Running unit tests for Pesto"
)

add_dependencies(pesto-unit-tests pesto_gtest)
