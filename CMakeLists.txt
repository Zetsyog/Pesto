#
# Pesto - A polyhedral loop nest optimizer
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2024 Clément Rossetti
#


cmake_minimum_required(VERSION 3.22)
project(pestoProject)

function(dump_cmake_variables)
    get_cmake_property(_variableNames VARIABLES)
    list(SORT _variableNames)
    foreach(_variableName ${_variableNames})
        if((NOT DEFINED ARGV0) OR _variableName MATCHES ${ARGV0})
            message(STATUS "${_variableName}=${${_variableName}}")
        endif()
    endforeach()
endfunction()

# Pesto version
set(PESTO_VERSION_MAJOR 0)
set(PESTO_VERSION_MINOR 1)
set(PESTO_VERSION_PATCH 0-dev)
set(PESTO_VERSION ${PESTO_VERSION_MAJOR}.${PESTO_VERSION_MINOR}.${PESTO_VERSION_PATCH})

set(PESTO_SOURCE_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(PESTO_BINARY_HEADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

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

# Set the C standard
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

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


# Miscellaneous dependencies

# json
include(cmake/json.cmake)

# muparser
include(cmake/muparser.cmake)

# gtest
include(cmake/gtest.cmake)

# spdlog
include(cmake/spdlog.cmake)

# Polyhedral libraries

# piplib
include(cmake/piplib.cmake)

# osl
include(cmake/osl.cmake)

# candl
include(cmake/candl.cmake)

# polylib
include(cmake/polylib.cmake)

# isl
include(cmake/isl.cmake)

# libpluto
include(cmake/libpluto.cmake)

# cloog
include(cmake/cloog.cmake)

# clan
include(cmake/clan.cmake)

# barvinok
include(cmake/barvinok.cmake)

# trahrhe
include(cmake/trahrhe.cmake)


# ##################################################################################################
# libpesto
# ##################################################################################################
add_subdirectory(lib)

# ##################################################################################################
# pesto-cli
# ##################################################################################################
add_subdirectory(cli)
set(PESTO_CLI_BIN "${CMAKE_BINARY_DIR}/cli/pesto")


# ##################################################################################################
# Tests
# ##################################################################################################
add_subdirectory(test EXCLUDE_FROM_ALL)

# TODO: Documentation if(DOXYGEN_FOUND) configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in
# ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY) add_custom_target(doc ${DOXYGEN_EXECUTABLE}
# ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMENT
# "Generating API documentation with Doxygen" VERBATIM ) endif()




# ##################################################################################################
# Installation
# ##################################################################################################

# Install libpesto
install(
    TARGETS pesto
    EXPORT pestoTargets
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/
)

# Install pesto-cli
install(
    TARGETS pesto-cli
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
# Install headers
install(
    DIRECTORY ${PESTO_SOURCE_HEADER_DIR}/
    DESTINATION include
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
)
install(
    DIRECTORY ${PESTO_BINARY_HEADER_DIR}/
    DESTINATION include
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
)

# install scripts
install(
    PROGRAMS #
    ${PESTO_CLI_BIN} #
    ${CMAKE_BINARY_DIR}/trahrhe_wrapper.sh #
    ${EXTRACT_SCOP_BIN} #
    ${REPLACE_SCOP_BIN}
    TYPE BIN
)

# Print configuration summary
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Linker flags: ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
