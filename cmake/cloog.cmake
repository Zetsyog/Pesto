#
# Pesto - A polyhedral loop nest optimizer
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2024 Clément Rossetti
#


# require isl
message(STATUS "Checking cloog-isl")

# Cloog patch files
set(CLOOG_PATCH_CLAST_USER_VEC
    ${CMAKE_SOURCE_DIR}/patches/cloog-Enhanced-CLAST_PARALLEL_USER-directive.patch
)
set(CLOOG_PATCH_CONFIGURE_FIX
    ${CMAKE_SOURCE_DIR}/patches/cloog-configure.ac-fix-TEXI2DVI-check-to-handle-missing-pr.patch
)

include(ExternalProject)

set(CLOOG_PREFIX ${CMAKE_BINARY_DIR}/_deps/cloog)
set(CLOOG_STATIC_LIB ${CLOOG_PREFIX}/install/lib/libcloog-isl.a)

file(MAKE_DIRECTORY ${CLOOG_PREFIX}/install/include)

externalproject_add(
    cloog-src
    GIT_REPOSITORY https://github.com/periscop/cloog.git
    GIT_TAG cloog-0.21.1 #
    GIT_SUBMODULES "" #
    PREFIX ${CLOOG_PREFIX}
    SOURCE_DIR ${CLOOG_PREFIX}/src
    INSTALL_DIR ${CLOOG_PREFIX}/install
    STAMP_DIR ${CLOOG_PREFIX}/stamp
    LOG_DIR ${CLOOG_PREFIX}/log
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
    --with-isl=system --with-isl-prefix=${ISL_PREFIX}/install
    --with-osl=system --with-osl-prefix=${OSL_PREFIX}/install
    --with-gmp=system
    --disable-shared
    PATCH_COMMAND
    ${CMAKE_COMMAND} -E echo "Applying ${CLOOG_PATCH_CLAST_USER_VEC}" && #
    patch -p1 < ${CLOOG_PATCH_CLAST_USER_VEC} && #
    ${CMAKE_COMMAND} -E echo "Applying ${CLOOG_PATCH_CONFIGURE_FIX}" && #
    patch -p1 < ${CLOOG_PATCH_CONFIGURE_FIX}
    BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} -j${PESTO_BUILD_JOBS}
    INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    UPDATE_DISCONNECTED true
)

add_dependencies(cloog-src isl-src osl-src)

add_library(cloog STATIC IMPORTED GLOBAL)
add_dependencies(cloog cloog-src)
set_target_properties(cloog PROPERTIES
    IMPORTED_LOCATION ${CLOOG_STATIC_LIB}
    INTERFACE_LINK_LIBRARIES ${CLOOG_STATIC_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${CLOOG_PREFIX}/install/include
)
