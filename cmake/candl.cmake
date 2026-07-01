#
# Pesto - A polyhedral loop nest optimizer
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2024 Clément Rossetti
#

message(STATUS "Checking Candl")

include(ExternalProject)
find_package(PkgConfig REQUIRED)

set(CANDL_PREFIX ${CMAKE_BINARY_DIR}/_deps/candl)
set(CANDL_STATIC_LIB ${CANDL_PREFIX}/install/lib/libcandl.a)

file(MAKE_DIRECTORY ${CANDL_PREFIX}/install/include)

# Dependencies
pkg_check_modules(GMP REQUIRED gmp)

externalproject_add(
    candl-src
    GIT_REPOSITORY https://github.com/periscop/candl.git
    GIT_TAG 6820f76b7b6b29222ba9cd1e6c0cc4e08b561dc8 # 
    GIT_SUBMODULES ""
    GIT_SUBMODULES_RECURSE false
    PREFIX ${CANDL_PREFIX}
    SOURCE_DIR ${CANDL_SRC_DIR}
    INSTALL_DIR ${CANDL_PREFIX}/install
    STAMP_DIR ${CANDL_PREFIX}/stamp
    LOG_DIR ${CANDL_PREFIX}/log
    BUILD_IN_SOURCE true
    CONFIGURE_BYPRODUCTS
    <BINARY_DIR>/config.status
    <BINARY_DIR>/Makefile

    CONFIGURE_ENVIRONMENT_MODIFICATION
    CFLAGS=set:${CMAKE_C_FLAGS}
    CPPFLAGS=set:-I${OSL_PREFIX}/install/include
    CXXFLAGS=set:${CMAKE_CXX_FLAGS}
    CONFIGURE_COMMAND
    COMMAND <SOURCE_DIR>/autogen.sh
    COMMAND <SOURCE_DIR>/configure
    CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} LD=${CMAKE_LINKER}
    CFLAGS=${CMAKE_C_FLAGS}
    CXXFLAGS=${CMAKE_CXX_FLAGS}
    --prefix=<INSTALL_DIR>
    --enable-llint-version
    --with-piplib=system --with-piplib-prefix=${PIPLIB_PREFIX}/install
    --with-osl=system --with-osl-prefix=${OSL_PREFIX}/install
    --disable-shared
    BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} -j${PESTO_BUILD_JOBS}
    INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    UPDATE_DISCONNECTED true
)
add_dependencies(candl-src piplib-src osl-src)

add_library(candl STATIC IMPORTED GLOBAL)
add_dependencies(candl candl-src)
set_target_properties(candl PROPERTIES
    IMPORTED_LOCATION ${CANDL_STATIC_LIB}
    INTERFACE_LINK_LIBRARIES ${CANDL_STATIC_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${CANDL_PREFIX}/install/include
)
target_link_libraries(candl INTERFACE osl piplib_dp)

