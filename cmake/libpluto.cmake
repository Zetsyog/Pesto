#
# Pesto - A polyhedral loop nest optimizer
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2024 Clément Rossetti
#


# require isl
message(STATUS "Checking libpluto")

include(ExternalProject)

# Dependencies
pkg_check_modules(GMP REQUIRED gmp)

# Pluto patch files
set(PLUTO_PATCH_COMPILE_FIX ${CMAKE_SOURCE_DIR}/patches/pluto_doc_gen_fix.patch)
set(PLUTO_PATCH_FUNC_DEF_FIX
    ${CMAKE_SOURCE_DIR}/patches/pluto-fix_multiple_definition_of_function.patch
)

set(PLUTO_PREFIX ${CMAKE_BINARY_DIR}/_deps/pluto)
set(PLUTO_STATIC_LIB ${PLUTO_PREFIX}/install/lib/libpluto.a)

file(MAKE_DIRECTORY ${PLUTO_PREFIX}/install/include)

externalproject_add(
    pluto-src
    GIT_REPOSITORY https://gitlab.inria.fr/crossett/pluto-openscop.git
    GIT_TAG dacbfd98ee9b71de6c7ed52c1c68495a972937b4 #
    GIT_SUBMODULES ""
    GIT_SUBMODULES_RECURSE false
    PREFIX ${PLUTO_PREFIX}
    SOURCE_DIR ${PLUTO_PREFIX}/src
    INSTALL_DIR ${PLUTO_PREFIX}/install
    STAMP_DIR ${PLUTO_PREFIX}/stamp
    LOG_DIR ${PLUTO_PREFIX}/log
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
    --with-isl-prefix=${ISL_PREFIX}/install
    --with-osl-prefix=${OSL_PREFIX}/install
    --with-piplib-prefix=${PIPLIB_PREFIX}/install
    --with-candl-prefix=${CANDL_PREFIX}/install
    --with-polylib-prefix=${POLYLIB_PREFIX}/install
    --disable-polycc
    --disable-shared
    PATCH_COMMAND ""
    BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} -j${PESTO_BUILD_JOBS}
    INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    UPDATE_DISCONNECTED true
)
add_dependencies(pluto-src piplib-src osl-src candl-src polylib-src isl-src)

add_library(pluto STATIC IMPORTED GLOBAL)
add_dependencies(pluto pluto-src)
set_target_properties(pluto PROPERTIES
    IMPORTED_LOCATION ${PLUTO_STATIC_LIB}
    INTERFACE_LINK_LIBRARIES ${PLUTO_STATIC_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${PLUTO_PREFIX}/install/include
)

target_link_libraries(pluto INTERFACE isl osl candl piplib_dp polylib-gmp ${GMP_LIBRARIES})
