# require isl
message(STATUS "Checking clan")

include(ExternalProject)

set(CLAN_PREFIX ${CMAKE_BINARY_DIR}/_deps/clan)
set(CLAN_STATIC_LIB ${CLAN_PREFIX}/install/lib/libclan.a)

file(MAKE_DIRECTORY ${CLAN_PREFIX}/install/include)

externalproject_add(
    clan-src
    GIT_REPOSITORY https://github.com/periscop/clan.git
    GIT_TAG 0.8.1 #
    GIT_SUBMODULES_RECURSE false
    GIT_SUBMODULES ""
    PREFIX ${CLAN_PREFIX}
    SOURCE_DIR ${CLAN_PREFIX}/src
    INSTALL_DIR ${CLAN_PREFIX}/install
    STAMP_DIR ${CLAN_PREFIX}/stamp
    LOG_DIR ${CLAN_PREFIX}/log
    BUILD_IN_SOURCE true
    CONFIGURE_BYPRODUCTS
    <BINARY_DIR>/config.status
    <BINARY_DIR>/Makefile
    CONFIGURE_ENVIRONMENT_MODIFICATION
    CONFIGURE_COMMAND
    COMMAND
    <SOURCE_DIR>/autogen.sh
    COMMAND
    <SOURCE_DIR>/configure
    CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} LD=${CMAKE_LINKER}
    CFLAGS=${CMAKE_C_FLAGS}
    CXXFLAGS=${CMAKE_CXX_FLAGS}
    LIBS=-lgmp # clan's configure script doesn't properly detect gmp, so we have to force it to link against it
    --prefix=<INSTALL_DIR> --with-osl=system
    --with-osl-prefix=${OSL_PREFIX}/install
    BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} -j${PESTO_BUILD_JOBS}
    INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    UPDATE_DISCONNECTED true
)

add_dependencies(clan-src osl-src)

add_library(clan STATIC IMPORTED GLOBAL)
add_dependencies(clan clan-src osl)
set_target_properties(clan PROPERTIES
    IMPORTED_LOCATION ${CLAN_STATIC_LIB}
    INTERFACE_LINK_LIBRARIES ${CLAN_STATIC_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${CLAN_PREFIX}/install/include
)
