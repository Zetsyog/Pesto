message(STATUS "Checking OpenScop Library (OSL)")

include(ExternalProject)
find_package(PkgConfig REQUIRED)

set(OSL_PREFIX ${CMAKE_BINARY_DIR}/_deps/osl)
set(OSL_STATIC_LIB ${OSL_PREFIX}/install/lib/libosl.a)

file(MAKE_DIRECTORY ${OSL_PREFIX}/install/include)

# Dependencies
pkg_check_modules(GMP REQUIRED gmp)

externalproject_add(
    osl-src
    GIT_REPOSITORY https://github.com/periscop/openscop.git
    GIT_TAG c60c47a5867f2a6973ef43348c2c08666244a797 # 
    PREFIX ${OSL_PREFIX}
    SOURCE_DIR ${OSL_PREFIX}/src
    INSTALL_DIR ${OSL_PREFIX}/install
    STAMP_DIR ${OSL_PREFIX}/stamp
    LOG_DIR ${OSL_PREFIX}/log
    BUILD_IN_SOURCE true
    CONFIGURE_BYPRODUCTS
    "<BINARY_DIR>/config.status"
    "<BINARY_DIR>/Makefile"
    CONFIGURE_COMMAND
    COMMAND
    <SOURCE_DIR>/autogen.sh
    COMMAND
    <SOURCE_DIR>/configure
    CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} LD=${CMAKE_LINKER}
    CFLAGS=${CMAKE_C_FLAGS}
    CXXFLAGS=${CMAKE_CXX_FLAGS}
    LDFLAGS=${CMAKE_EXE_LINKER_FLAGS}
    --prefix=${OSL_PREFIX}/install
    --disable-shared
    BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} -j${PESTO_BUILD_JOBS}
    INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    UPDATE_DISCONNECTED true
)

add_library(osl STATIC IMPORTED GLOBAL)
add_dependencies(osl osl-src)
set_target_properties(osl PROPERTIES
    IMPORTED_LOCATION ${OSL_STATIC_LIB}
    INTERFACE_LINK_LIBRARIES ${OSL_STATIC_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${OSL_PREFIX}/install/include
)
