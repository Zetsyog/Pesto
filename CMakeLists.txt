cmake_minimum_required(VERSION 3.15)
project(pestoProject)

# Pesto version
set(PESTO_VERSION_MAJOR 0)
set(PESTO_VERSION_MINOR 1)
set(PESTO_VERSION_PATCH 0-dev)
set(PESTO_VERSION
	${PESTO_VERSION_MAJOR}.${PESTO_VERSION_MINOR}.${PESTO_VERSION_PATCH})

# Number of parallel builds to run (default: 1)
set(PESTO_BUILD_JOBS
	1
	CACHE STRING "the number of parallel jobs used to build Pesto")

message(STATUS "Pesto will be compiled using up to ${PESTO_BUILD_JOBS} job(s)")

# Default to Release build type
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

include(ExternalProject)
include(FetchContent)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include/)

# ##############################################################################
# Dependencies
# ##############################################################################
find_package(PkgConfig REQUIRED)
find_package(Git REQUIRED)
find_package(Doxygen)
find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

# pkg-config packages
pkg_check_modules(GMP REQUIRED gmp)

# ##############################################################################
# pluto
# ##############################################################################
set(PLUTO_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/pluto-openscop-install")

# Pluto patch files
set(PLUTO_PATCH_COMPILE_FIX ${CMAKE_SOURCE_DIR}/patches/pluto_doc_gen_fix.patch)
set(PLUTO_PATCH_CLOOG_USER_PRAGMA
	${CMAKE_SOURCE_DIR}/patches/cloog_better_user_directive.patch)
set(PLUTO_PATCH_PAR_LOOP_FIX
	${CMAKE_SOURCE_DIR}/patches/pluto_osl_par_loop_fix.patch)
ExternalProject_Add(
	pluto-openscop
	GIT_REPOSITORY https://gitlab.inria.fr/crossett/pluto-openscop.git
	GIT_TAG origin/main
	PREFIX ${CMAKE_BINARY_DIR}/pluto-openscop
	GIT_SUBMODULES_RECURSE false
	UPDATE_DISCONNECTED true
	PATCH_COMMAND
		${CMAKE_COMMAND} -E echo "Applying ${PLUTO_PATCH_COMPILE_FIX}" && #
		patch -p1 < ${PLUTO_PATCH_COMPILE_FIX} && #
		${CMAKE_COMMAND} -E echo "Applying ${PLUTO_PATCH_CLOOG_USER_PRAGMA}" &&
		patch -p1 < ${PLUTO_PATCH_CLOOG_USER_PRAGMA} && #
		${CMAKE_COMMAND} -E echo "Applying ${PLUTO_PATCH_PAR_LOOP_FIX}" && #
		patch -p1 < ${PLUTO_PATCH_PAR_LOOP_FIX}
	CONFIGURE_COMMAND ./autogen.sh && ./configure --prefix=${PLUTO_INSTALL_DIR}
	BUILD_IN_SOURCE ON
	BUILD_COMMAND make -j${PESTO_BUILD_JOBS}
	INSTALL_COMMAND make install)

# ##############################################################################
# trahrhe
# ##############################################################################

# then we can build it
add_custom_target(
	trahrhe
	COMMAND make
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/trahrhe-dev)

# ##############################################################################
# muparser
# ##############################################################################
FetchContent_Declare(
	muparser
	GIT_REPOSITORY https://github.com/beltoforion/muparser.git
	GIT_TAG 59e0ce1e0d35d42713af67788f19797945d81364 # v2.3.4
	EXCLUDE_FROM_ALL)
if(CMAKE_VERSION VERSION_LESS "3.28")
	FetchContent_GetProperties(muparser)
	if(NOT muparser_POPULATED)
		FetchContent_Populate(muparser)
		add_subdirectory(${muparser_SOURCE_DIR} ${muparser_BINARY_DIR}
						 EXCLUDE_FROM_ALL)
	endif()
else()
	FetchContent_MakeAvailable(muparser)
endif()

# ##############################################################################
# Google Test
# ##############################################################################
FetchContent_Declare(
	googletest
	GIT_REPOSITORY https://github.com/google/googletest.git
	GIT_TAG b514bdc898e2951020cbdca1304b75f5950d1f59 # v1.15.2
	EXCLUDE_FROM_ALL)
if(CMAKE_VERSION VERSION_LESS "3.28")
	FetchContent_GetProperties(googletest)
	if(NOT googletest_POPULATED)
		FetchContent_Populate(googletest)
		add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR}
						 EXCLUDE_FROM_ALL)
	endif()
else()
	FetchContent_MakeAvailable(googletest)
endif()

# ##############################################################################
# nlohmann_json
# ##############################################################################
FetchContent_Declare(
	nlohmann_json
	GIT_REPOSITORY https://github.com/nlohmann/json.git
	GIT_TAG bc889afb4c5bf1c0d8ee29ef35eaaf4c8bef8a5d # v3.11.2
	EXCLUDE_FROM_ALL)
if(CMAKE_VERSION VERSION_LESS "3.28")
	FetchContent_GetProperties(nlohmann_json)
	if(NOT nlohmann_json_POPULATED)
		FetchContent_Populate(nlohmann_json)
		add_subdirectory(${nlohmann_json_SOURCE_DIR}
						 ${nlohmann_json_BINARY_DIR} EXCLUDE_FROM_ALL)
	endif()
else()
	FetchContent_MakeAvailable(nlohmann_json)
endif()

# ##############################################################################
# libpesto
# ##############################################################################

# Sources files
file(GLOB_RECURSE LIBPESTO_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.cpp"
	 "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.c")

# Add libraries
add_library(pesto STATIC ${LIBPESTO_SOURCES})

add_dependencies(pesto pluto-openscop)

# GMP dependency
target_link_libraries(pesto PUBLIC ${GMP_LIBRARIES})

# pluto-openscop dependency
target_link_libraries(pesto PUBLIC ${PLUTO_INSTALL_DIR}/lib/libpluto.a)
target_include_directories(pesto PUBLIC ${PLUTO_INSTALL_DIR}/include)

# isl dependency
target_link_libraries(pesto PUBLIC ${PLUTO_INSTALL_DIR}/lib/libisl.a)

# osl dependency
target_link_libraries(pesto PUBLIC ${PLUTO_INSTALL_DIR}/lib/libosl.a)

# cloog dependency
target_link_libraries(pesto PUBLIC ${PLUTO_INSTALL_DIR}/lib/libcloog-isl.a)

# candl dependency
target_link_libraries(pesto PUBLIC ${PLUTO_INSTALL_DIR}/lib/libcandl.a)

# piplib dependency
target_link_libraries(pesto PUBLIC ${PLUTO_INSTALL_DIR}/lib/libpiplib_dp.a)

# muparser dependency
target_link_libraries(pesto PUBLIC muparser)
target_include_directories(pesto PUBLIC ${muparser_INCLUDE_DIRS})

# nlohmann_json dependency
target_link_libraries(pesto PRIVATE nlohmann_json::nlohmann_json)

# ##############################################################################
# pestotool
# ##############################################################################

# Sources files
file(GLOB_RECURSE PESTOTOOL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tool/*.cpp"
	 "${CMAKE_CURRENT_SOURCE_DIR}/tool/*.c")

# Add executables
add_executable(pestotool ${PESTOTOOL_SOURCES})

# trahrhe dependency
add_dependencies(pestotool trahrhe)

# libpesto dependency
add_dependencies(pestotool pesto)
target_link_libraries(pestotool PRIVATE pesto)

# pluto include
target_include_directories(pestotool PRIVATE ${PLUTO_INSTALL_DIR}/include)

# clan dependency
target_link_libraries(pestotool PRIVATE ${PLUTO_INSTALL_DIR}/lib/libclan.a)

# cloog dependency
target_link_libraries(pestotool PRIVATE ${PLUTO_INSTALL_DIR}/lib/libcloog-isl.a)

# muparser dependency
target_link_libraries(pestotool PRIVATE muparser)
target_include_directories(pestotool PRIVATE ${muparser_INCLUDE_DIRS})

# nlohmann_json dependency
target_link_libraries(pestotool PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(pestotool PRIVATE ${nlohmann_json_INCLUDE_DIRS})

# ##############################################################################
# pesto-cli
# ##############################################################################

# Define variable for the templates
set(EXTRACT_SCOP_BIN "${CMAKE_BINARY_DIR}/extract_scop.sh")
set(REPLACE_SCOP_BIN "${CMAKE_BINARY_DIR}/replace_scop.sh")
set(TRAHRHE_WRAPPER_BIN "${CMAKE_BINARY_DIR}/trahrhe_wrapper.sh")
set(PESTO_TOOL_BIN "${CMAKE_BINARY_DIR}/pestotool")
set(TRAHRHE_INSTALL_DIR "${CMAKE_SOURCE_DIR}/trahrhe-dev")

# Add bash scripts
configure_file("${CMAKE_SOURCE_DIR}/extract_scop.sh" "${CMAKE_BINARY_DIR}"
			   COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/replace_scop.sh" "${CMAKE_BINARY_DIR}"
			   COPYONLY)

configure_file(
	"${CMAKE_SOURCE_DIR}/pesto.in.sh" # input template file
	"${CMAKE_BINARY_DIR}/pesto" # output file
	FILE_PERMISSIONS
	OWNER_READ
	OWNER_WRITE
	OWNER_EXECUTE
	GROUP_READ
	GROUP_EXECUTE
	WORLD_READ
	WORLD_EXECUTE
	@ONLY)
configure_file(
	"${CMAKE_SOURCE_DIR}/trahrhe_wrapper.in.sh" # input template file
	"${CMAKE_BINARY_DIR}/trahrhe_wrapper.sh" # output file
	FILE_PERMISSIONS
	OWNER_READ
	OWNER_WRITE
	OWNER_EXECUTE
	GROUP_READ
	GROUP_EXECUTE
	WORLD_READ
	WORLD_EXECUTE
	@ONLY)

# TODO Add tests
add_subdirectory(test EXCLUDE_FROM_ALL)

# TODO Documentation if(DOXYGEN_FOUND)
# configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in
# ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY) add_custom_target(doc
# ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile WORKING_DIRECTORY
# ${CMAKE_CURRENT_BINARY_DIR} COMMENT "Generating API documentation with
# Doxygen" VERBATIM ) endif()

# Install targets
install(
	TARGETS pesto pestotool
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib)
# Install headers
install(
	DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
	DESTINATION include
	FILES_MATCHING
	PATTERN "*.h"
	PATTERN "*.hpp")
# Install trahrhe
install(DIRECTORY ${CMAKE_BINARY_DIR}/trahrhe/ DESTINATION trahrhe)
# install scripts
install(
	PROGRAMS ${CMAKE_BINARY_DIR}/pesto ${CMAKE_BINARY_DIR}/trahrhe_wrapper.sh
			 ${CMAKE_BINARY_DIR}/extract_scop.sh
			 ${CMAKE_BINARY_DIR}/replace_scop.sh
	DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

# Print configuration summary
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Linker flags: ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
