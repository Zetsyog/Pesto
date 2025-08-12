include(ExternalProject)

# trahrhe
message(STATUS "Fetchin trahrhe")

# Check if trahrhe dependencies are installed
find_program(MAXIMA "maxima" REQUIRED)

set(TRAHRHE_GIT_URL "https://gitlab.inria.fr/crossett/trahrhe-blitzcrank.git")
set(TRAHRHE_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/trahrhe")

message(STATUS "Barvinok path: ${BARVINOK_INSTALL_DIR}")

message(STATUS "Using latest trahrhe from git")
ExternalProject_Add(
	trahrhe-src
	GIT_REPOSITORY ${TRAHRHE_GIT_URL}
	UPDATE_DISCONNECTED ON
	GIT_TAG bee4e75ef06d271899263b5338361acdde421311
	CMAKE_CACHE_ARGS
		"-DBARVINOK_PATH:PATH=${BARVINOK_INSTALL_DIR}" #
		"-DISL_INSTALL_PATH:PATH=${PLUTO_PREFIX}" #
		"-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/trahrhe" #
	PREFIX ${TRAHRHE_PREFIX}
)
link_directories(${TRAHRHE_PREFIX}/lib)
link_directories(${TRAHRHE_PREFIX}/lib64)

add_dependencies(trahrhe-src barvinok-src)

set(TRAHRHE_INCLUDE_DIRS ${TRAHRHE_PREFIX}/include)
set(TRAHRHE_LIBRARIES ${BARVINOK_LIBRARIES} trahrhe m)

dump_cmake_variables("TRAHRHE_.*")
dump_cmake_variables("trahrhe_.*")
