include(ExternalProject)

message(STATUS "Fetching Barvinok")

find_package(PkgConfig REQUIRED)
find_package(NTL REQUIRED)
find_package(GLPK)

# Pkg-config dependencies
pkg_check_modules(GMP REQUIRED gmp)

pkg_search_module(CDDLIB cddgmp cddlib)
pkg_check_modules(CDDLIB ${CDDLIB_MODULE_NAME})


if(${CDDLIB_FOUND})
    message(STATUS "CDDLIB: found ${CDDLIB_MODULE_NAME}")
endif()

if(NOT NTL_FOUND)
    message(FATAL_ERROR "NTL library not found. Please install NTL.")
endif()

set(BARVINOK_VERSION 0.41.8)
set(BARVINOK_PREFIX ${CMAKE_BINARY_DIR}/_deps/barvinok)
set(BARVINOK_INSTALL_DIR
    "${BARVINOK_PREFIX}/install"
    CACHE INTERNAL "Path to Barvinok installation directory"
)

file(MAKE_DIRECTORY ${BARVINOK_PREFIX}/install/include)

# Locate the Barvinok library
externalproject_add(
    barvinok-src
    GIT_REPOSITORY https://repo.or.cz/barvinok.git
    GIT_TAG barvinok-${BARVINOK_VERSION}
    GIT_SUBMODULES ""
    PREFIX ${BARVINOK_PREFIX}
    SOURCE_DIR ${BARVINOK_PREFIX}/src
    INSTALL_DIR ${BARVINOK_PREFIX}/install
    STAMP_DIR ${BARVINOK_PREFIX}/stamp
    LOG_DIR ${BARVINOK_PREFIX}/log
    BUILD_IN_SOURCE true
    DOWNLOAD_EXTRACT_TIMESTAMP true
    CONFIGURE_COMMAND
    COMMAND
    <SOURCE_DIR>/autogen.sh
    COMMAND
    <SOURCE_DIR>/configure
    CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} LD=${CMAKE_LINKER}
    CFLAGS=${CMAKE_C_FLAGS}
    CXXFLAGS=${CMAKE_CXX_FLAGS}
    --prefix=${BARVINOK_INSTALL_DIR}
    --with-isl=system --with-isl-prefix=${ISL_PREFIX}/install
    --with-polylib=system --with-polylib-prefix=${POLYLIB_PREFIX}/install
    --with-pet=no
    --with-glpk=no
    --disable-shared-barvinok
    BUILD_COMMAND make -j${PESTO_BUILD_JOBS}
    INSTALL_COMMAND make install
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    UPDATE_DISCONNECTED true
)

add_dependencies(barvinok-src isl-src polylib-src)

add_library(barvinok STATIC IMPORTED GLOBAL)
add_dependencies(barvinok barvinok-src)
set_target_properties(barvinok PROPERTIES
    IMPORTED_LOCATION ${BARVINOK_INSTALL_DIR}/lib/libbarvinok.a
    INTERFACE_LINK_LIBRARIES ${BARVINOK_INSTALL_DIR}/lib/libbarvinok.a
    INTERFACE_INCLUDE_DIRECTORIES ${BARVINOK_INSTALL_DIR}/include
)
target_link_libraries(barvinok INTERFACE isl polylib-gmp ${CDDLIB_LIBRARIES} ${NTL_LIBRARIES} ${GMP_LIBRARIES})
