# Google Test
FetchContent_Declare(
	googletest
	GIT_REPOSITORY https://github.com/google/googletest.git
	GIT_TAG b514bdc898e2951020cbdca1304b75f5950d1f59 # v1.15.2
	EXCLUDE_FROM_ALL
)
if(CMAKE_VERSION VERSION_LESS "3.28")
	FetchContent_GetProperties(googletest)
	if(NOT googletest_POPULATED)
		FetchContent_Populate(googletest)
		add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
	endif()
else()
	FetchContent_MakeAvailable(googletest)
endif()
