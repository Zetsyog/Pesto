include(ExternalProject)

# trahrhe
message(STATUS "Fetchin trahrhe")

# Check if trahrhe dependencies are installed
find_program(MAXIMA "maxima" REQUIRED)

set(TRAHRHE_GIT_URL "https://gitlab.inria.fr/crossett/trahrhe-blitzcrank.git")
set(TRAHRHE_PREFIX "${CMAKE_BINARY_DIR}/_deps/trahrhe")

file(MAKE_DIRECTORY ${TRAHRHE_PREFIX}/install/include)

message(STATUS "Barvinok path: ${BARVINOK_INSTALL_DIR}")

message(STATUS "Using latest trahrhe from git")
externalproject_add(
    trahrhe-src
    GIT_REPOSITORY ${TRAHRHE_GIT_URL}
    GIT_TAG f1ab9295a732782728ed06e3d24eee90eb7e9841
    PREFIX ${TRAHRHE_PREFIX}
    SOURCE_DIR ${TRAHRHE_PREFIX}/src
    BINARY_DIR ${TRAHRHE_PREFIX}/build
    INSTALL_DIR ${TRAHRHE_PREFIX}/install
    STAMP_DIR ${TRAHRHE_PREFIX}/stamp
    LOG_DIR ${TRAHRHE_PREFIX}/log
    CMAKE_CACHE_ARGS
    "-DCMAKE_C_FLAGS:PATH=-I${POLYLIB_PREFIX}/install/include -L${POLYLIB_PREFIX}/install/lib"
    "-DBARVINOK_PATH:PATH=${BARVINOK_INSTALL_DIR}"
    "-DISL_INSTALL_PATH:PATH=${ISL_PREFIX}/install"
    "-DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>"
    BUILD_BYPRODUCTS ${TRAHRHE_PREFIX}/install/bin/trahrhe
    UPDATE_DISCONNECTED OFF
)
add_dependencies(trahrhe-src barvinok-src isl-src polylib-src)

add_library(trahrhe STATIC IMPORTED GLOBAL)
add_dependencies(trahrhe trahrhe-src)
set_target_properties(trahrhe PROPERTIES
    IMPORTED_LOCATION ${TRAHRHE_PREFIX}/install/lib64/libtrahrhe.a
    INTERFACE_LINK_LIBRARIES ${TRAHRHE_PREFIX}/install/lib64/libtrahrhe.a
    INTERFACE_INCLUDE_DIRECTORIES ${TRAHRHE_PREFIX}/install/include
)
target_link_libraries(trahrhe INTERFACE barvinok polylib-gmp)
