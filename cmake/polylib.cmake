message(STATUS "Checking Polylib")

include(ExternalProject)
find_package(PkgConfig REQUIRED)

set(POLYLIB_PREFIX ${CMAKE_BINARY_DIR}/_deps/polylib)
set(POLYLIB64_STATIC_LIB ${POLYLIB_PREFIX}/install/lib/libpolylib64.a)
set(POLYLIBGMP_STATIC_LIB ${POLYLIB_PREFIX}/install/lib/libpolylibgmp.a)

file(MAKE_DIRECTORY ${POLYLIB_PREFIX}/install/include)

# Dependencies
pkg_check_modules(GMP REQUIRED gmp)

externalproject_add(
    polylib-src
    GIT_REPOSITORY https://github.com/vincentloechner/polylib.git
    GIT_TAG 9822337ad3f75477a4c8d860e361d92e47bffe0c # 
    PREFIX ${POLYLIB_PREFIX}
    SOURCE_DIR ${POLYLIB_PREFIX}/src
    INSTALL_DIR ${POLYLIB_PREFIX}/install
    STAMP_DIR ${POLYLIB_PREFIX}/stamp
    LOG_DIR ${POLYLIB_PREFIX}/log
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
    --enable-longlongint-lib
    --with-libgmp=${GMP_PREFIX}
    --disable-shared
    BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} -j${PESTO_BUILD_JOBS}
    INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    UPDATE_DISCONNECTED true
)

add_library(polylib-64 STATIC IMPORTED GLOBAL)
add_dependencies(polylib-64 polylib-src)
set_target_properties(polylib-64 PROPERTIES
    IMPORTED_LOCATION ${POLYLIB64_STATIC_LIB}
    INTERFACE_LINK_LIBRARIES ${POLYLIB64_STATIC_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${POLYLIB_PREFIX}/install/include
)

add_library(polylib-gmp STATIC IMPORTED GLOBAL)
add_dependencies(polylib-gmp polylib-src)
set_target_properties(polylib-gmp PROPERTIES
    IMPORTED_LOCATION ${POLYLIBGMP_STATIC_LIB}
    INTERFACE_LINK_LIBRARIES ${POLYLIBGMP_STATIC_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${POLYLIB_PREFIX}/install/include
)
