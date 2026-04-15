message(STATUS "Checking piplib")

include(ExternalProject)
find_package(PkgConfig REQUIRED)

set(PIPLIB_PREFIX ${CMAKE_BINARY_DIR}/_deps/piplib)
set(PIPLIB_STATIC_LIB ${PIPLIB_PREFIX}/install/lib/libpiplib_dp.a)

file(MAKE_DIRECTORY ${PIPLIB_PREFIX}/install/include)

# Dependencies
pkg_check_modules(GMP REQUIRED gmp)

externalproject_add(
    piplib-src
    GIT_REPOSITORY https://github.com/periscop/piplib.git
    GIT_TAG 89090bfd8497c5a196ec1b1e3f314d9f8e20dacb # 
    PREFIX ${PIPLIB_PREFIX}
    SOURCE_DIR ${PIPLIB_PREFIX}/src
    INSTALL_DIR ${PIPLIB_PREFIX}/install
    STAMP_DIR ${PIPLIB_PREFIX}/stamp
    LOG_DIR ${PIPLIB_PREFIX}/log
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
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    UPDATE_DISCONNECTED true

)

add_library(piplib_dp STATIC IMPORTED GLOBAL)
add_dependencies(piplib_dp piplib-src)
set_target_properties(piplib_dp PROPERTIES
    IMPORTED_LOCATION ${PIPLIB_PREFIX}/install/lib/libpiplib_dp.a
    INTERFACE_LINK_LIBRARIES ${PIPLIB_PREFIX}/install/lib/libpiplib_dp.a
    INTERFACE_INCLUDE_DIRECTORIES ${PIPLIB_PREFIX}/install/include
)
add_library(piplib_gmp STATIC IMPORTED GLOBAL)
add_dependencies(piplib_gmp piplib-src)
set_target_properties(piplib_gmp PROPERTIES
    IMPORTED_LOCATION ${PIPLIB_PREFIX}/install/lib/libpiplib_gmp.a
    INTERFACE_LINK_LIBRARIES ${PIPLIB_PREFIX}/install/lib/libpiplib_gmp.a
    INTERFACE_INCLUDE_DIRECTORIES ${PIPLIB_PREFIX}/install/include

)

