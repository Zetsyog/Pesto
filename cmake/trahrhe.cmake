include(FetchContent)

# trahrhe
message(STATUS "Fetchin trahrhe")

# Check if trahrhe dependencies are installed
find_program(BARVINOK_ISCC "iscc" REQUIRED)
find_program(MAXIMA "maxima" REQUIRED)

set(TRAHRHE_DIR ${CMAKE_BINARY_DIR}/trahrhe)

option(PESTO_TRAHRHE_VERSION "Trahrhe version to use" "5.0")

if(${PESTO_TRAHRHE_VERSION} VERSION_LESS 5.0)
	set(TRAHRHE_IS_LEGACY true)
else()
	set(TRAHRHE_IS_LEGACY false)
endif()

set(TRAHRHE_URL
	https://webpages.gitlabpages.inria.fr/trahrhe/trahrhe_packages/trahrhe-${PESTO_TRAHRHE_VERSION}.zip
)
if(TRAHRHE_IS_LEGACY)
	message(STATUS "Using trahrhe ${PESTO_TRAHRHE_VERSION} (legacy)")
	set(TRAHRHE_BUILD_COMMAND ${CMAKE_MAKE_PROGRAM})
else()
	message(STATUS "Using trahrhe ${PESTO_TRAHRHE_VERSION}")

	find_program(BASH_EXECUTABLE NAMES bash)
	find_program(SH_EXECUTABLE NAMES sh)

	if(BASH_EXECUTABLE)
		set(TRAHRHE_SHELL ${BASH_EXECUTABLE})
	elseif(SH_EXECUTABLE)
		set(TRAHRHE_SHELL ${SH_EXECUTABLE})
	else()
		message(FATAL_ERROR "No suitable shell found")
	endif()

	set(TRAHRHE_BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} SHELL=${TRAHRHE_SHELL} all)
endif()

FetchContent_Declare(
	trahrhe
	DOWNLOAD_EXTRACT_TIMESTAMP true EXCLUDE_FROM_ALL
	URL ${TRAHRHE_URL} DOWNLOAD_DIR ${TRAHRHE_DIR} SOURCE_DIR ${TRAHRHE_DIR}
)
if(CMAKE_VERSION VERSION_LESS "3.28")
	FetchContent_GetProperties(trahrhe)
	if(NOT trahrhe_POPULATED)
		FetchContent_Populate(trahrhe)
	endif()
else()
	FetchContent_MakeAvailable(trahrhe)
endif()

message(STATUS "Building trahrhe")
execute_process(
	COMMAND ${TRAHRHE_BUILD_COMMAND}
	WORKING_DIRECTORY ${TRAHRHE_DIR}
	OUTPUT_QUIET ERROR_QUIET
)
