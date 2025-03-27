include(FetchContent)

# trahrhe
message(STATUS "Fetchin trahrhe")

# Check if trahrhe dependencies are installed
find_program(BARVINOK_ISCC "iscc" REQUIRED)
find_program(MAXIMA "maxima" REQUIRED)

set(TRAHRHE_DIR ${CMAKE_BINARY_DIR}/trahrhe)

# default trahrhe version
set(PESTO_TRAHRHE_VERSION
	5.0
	CACHE STRING "Trahrhe version to use"
)

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

if(NOT TRAHRHE_IS_LEGACY)
	# Quick fix the makefile
	message(STATUS "Fixing trahrhe makefile")
	execute_process(COMMAND sed -i "s/-lglpk//g" ${TRAHRHE_DIR}/makefile)
endif()

# if trahrhe is version 5.0 we need to apply a patch
if(PESTO_TRAHRHE_VERSION VERSION_EQUAL 5.0)
	message(STATUS "Applying trahrhe buffer overflow patch for version 5.0")
	execute_process(
		COMMAND patch -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/patches/trahrhe-5.0-buffer-overflow.patch
		WORKING_DIRECTORY ${TRAHRHE_DIR}
	)
endif()

message(STATUS "Building trahrhe")
execute_process(
	COMMAND ${TRAHRHE_BUILD_COMMAND}
	WORKING_DIRECTORY ${TRAHRHE_DIR}
	OUTPUT_QUIET ERROR_QUIET
	RESULT_VARIABLE TRAHRHE_BUILD_RESULT
)

if(NOT TRAHRHE_BUILD_RESULT EQUAL 0)
	message(FATAL_ERROR "Failed to build trahrhe")
endif()
