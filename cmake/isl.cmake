#
# Pesto - A polyhedral loop nest optimizer
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2024 Clément Rossetti
#


message(STATUS "Checking Integer Set Library (ISL)")

include(ExternalProject)
find_package(PkgConfig REQUIRED)

set(ISL_PREFIX ${CMAKE_BINARY_DIR}/_deps/isl)
set(ISL_STATIC_LIB ${ISL_PREFIX}/install/lib/libisl.a)

file(MAKE_DIRECTORY ${ISL_PREFIX}/install/include)

# Dependencies
pkg_check_modules(GMP REQUIRED gmp)

externalproject_add(
    isl-src
    GIT_REPOSITORY https://github.com/bondhugula/isl-for-pluto.git
    GIT_TAG 0114734350a32cbc6811cfc12693f3f40d14621b # 
    PREFIX ${ISL_PREFIX}
    SOURCE_DIR ${ISL_PREFIX}/src
    INSTALL_DIR ${ISL_PREFIX}/install
    STAMP_DIR ${ISL_PREFIX}/stamp
    LOG_DIR ${ISL_PREFIX}/log
    BUILD_IN_SOURCE true
    CONFIGURE_BYPRODUCTS
    <BINARY_DIR>/config.status
    <BINARY_DIR>/Makefile
    CONFIGURE_COMMAND
    COMMAND
    <SOURCE_DIR>/autogen.sh
    COMMAND
    <SOURCE_DIR>/configure
    CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} LD=${CMAKE_LINKER}
    CFLAGS=${CMAKE_C_FLAGS}
    CXXFLAGS=${CMAKE_CXX_FLAGS}
    --prefix=<INSTALL_DIR>
    --disable-shared
    BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} -j${PESTO_BUILD_JOBS}
    INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
    LOG_DOWNLOAD ON
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    UPDATE_DISCONNECTED ON
)

add_library(isl STATIC IMPORTED GLOBAL)
add_dependencies(isl isl-src)
set_target_properties(isl PROPERTIES
    IMPORTED_LOCATION ${ISL_STATIC_LIB}
    INTERFACE_LINK_LIBRARIES ${ISL_STATIC_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${ISL_PREFIX}/install/include
)
