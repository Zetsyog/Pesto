cmake_minimum_required(VERSION 3.22)
project(pestoProject)

# Pesto version
set(PESTO_VERSION_MAJOR 0)
set(PESTO_VERSION_MINOR 1)
set(PESTO_VERSION_PATCH 0-dev)
set(PESTO_VERSION ${PESTO_VERSION_MAJOR}.${PESTO_VERSION_MINOR}.${PESTO_VERSION_PATCH})

# Number of parallel builds to run (default: 1)
set(PESTO_BUILD_JOBS
	1
	CACHE STRING "the number of parallel jobs used to build Pesto"
)

message(STATUS "Pesto will be compiled using up to ${PESTO_BUILD_JOBS} job(s)")

# Default to Release build type
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

include(FetchContent)

# ##################################################################################################
# Dependencies
# ##################################################################################################
find_package(PkgConfig REQUIRED)
find_package(Git REQUIRED)
find_package(Doxygen)
find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

# pkg-config packages
pkg_check_modules(GMP REQUIRED gmp)

# libpluto
include(cmake/libpluto.cmake)

# cloog
include(cmake/cloog.cmake)

# clan
include(cmake/clan.cmake)

# json
include(cmake/json.cmake)

# muparser
include(cmake/muparser.cmake)

# gtest
include(cmake/gtest.cmake)

# trahrhe
include(cmake/trahrhe.cmake)

# ##################################################################################################
# libpesto
# ##################################################################################################
add_subdirectory(lib)

# ##################################################################################################
# pestobin
# ##################################################################################################
add_subdirectory(tool)

# ##################################################################################################
# pesto-cli
# ##################################################################################################

# Define variable for the templates
set(EXTRACT_SCOP_BIN "${CMAKE_BINARY_DIR}/extract_scop.sh")
set(REPLACE_SCOP_BIN "${CMAKE_BINARY_DIR}/replace_scop.sh")
set(TRAHRHE_WRAPPER_BIN "${CMAKE_BINARY_DIR}/trahrhe_wrapper.sh")
set(PESTO_TOOL_BIN "${CMAKE_BINARY_DIR}/pestobin")
set(PESTO_CLI_BIN "${CMAKE_BINARY_DIR}/pesto")

# Add bash scripts
configure_file("${CMAKE_SOURCE_DIR}/extract_scop.sh" "${CMAKE_BINARY_DIR}" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/replace_scop.sh" "${CMAKE_BINARY_DIR}" COPYONLY)

configure_file(
	"${CMAKE_SOURCE_DIR}/pesto.in.sh" # input template file
	${PESTO_CLI_BIN} # output file
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
configure_file(
	"${CMAKE_SOURCE_DIR}/trahrhe_wrapper.in.sh" # input template file
	"${CMAKE_BINARY_DIR}/trahrhe_wrapper.sh" # output file
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

add_custom_target(pesto-cli)
add_dependencies(pesto-cli pestobin)

# ##################################################################################################
# Tests
# ##################################################################################################
add_subdirectory(test EXCLUDE_FROM_ALL)

# TODO: Documentation if(DOXYGEN_FOUND) configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in
# ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY) add_custom_target(doc ${DOXYGEN_EXECUTABLE}
# ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMENT
# "Generating API documentation with Doxygen" VERBATIM ) endif()

# Install targets
install(
	TARGETS pesto pestobin
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
)
# Install headers
install(
	DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
	DESTINATION include
	FILES_MATCHING
	PATTERN "*.h"
	PATTERN "*.hpp"
)

# install scripts
install(PROGRAMS ${CMAKE_BINARY_DIR}/pesto ${CMAKE_BINARY_DIR}/trahrhe_wrapper.sh
				 ${CMAKE_BINARY_DIR}/extract_scop.sh ${CMAKE_BINARY_DIR}/replace_scop.sh
		DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)

# Print configuration summary
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Linker flags: ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
