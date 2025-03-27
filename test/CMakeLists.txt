# unit tests (Google Test)
add_subdirectory(GTest)

# Polybench tests
set(TEST_POLYBENCH_BIN "${CMAKE_CURRENT_BINARY_DIR}/test_polybench.sh")

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
