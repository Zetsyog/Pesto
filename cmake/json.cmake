# nlohmann_json
FetchContent_Declare(
	nlohmann_json
	GIT_REPOSITORY "https://github.com/nlohmann/json.git"
	GIT_TAG "v3.11.3"
	EXCLUDE_FROM_ALL
)
if(CMAKE_VERSION VERSION_LESS "3.28")
	FetchContent_GetProperties(nlohmann_json)
	if(NOT nlohmann_json_POPULATED)
		FetchContent_Populate(nlohmann_json)
		add_subdirectory(${nlohmann_json_SOURCE_DIR} ${nlohmann_json_BINARY_DIR} EXCLUDE_FROM_ALL)
	endif()
else()
	FetchContent_MakeAvailable(nlohmann_json)
endif()
