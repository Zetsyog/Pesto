# nlohmann_json
message(STATUS "Fetching nlohmann_json")

FetchContent_Declare(
	json #
	DOWNLOAD_EXTRACT_TIMESTAMP true #
	EXCLUDE_FROM_ALL
	URL "https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz"
)
FetchContent_MakeAvailable(json)
