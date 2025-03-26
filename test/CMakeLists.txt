# unit tests (Google Test)
add_subdirectory(GTest)

# Polybench tests
set(TEST_POLYBENCH_BIN "${CMAKE_CURRENT_BINARY_DIR}/test_polybench.sh")

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/test_polybench.sh.in
	${TEST_POLYBENCH_BIN}
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

add_custom_target(
	test-polybench
	COMMAND bash ${TEST_POLYBENCH_BIN} ${PESTO_CLI}
	DEPENDS pesto-cli
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/polybench-4.2.1/
)
