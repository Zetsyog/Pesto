cmake_minimum_required(VERSION 3.15)
project(pestoProject)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Default to Release build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(ExternalProject)

# Include directories
include_directories(
  ${CMAKE_SOURCE_DIR}/include/
)

###################################
# Dependencies
###################################
find_package(PkgConfig REQUIRED)
find_package(Doxygen)
find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

# pkg-config packages
pkg_check_modules(GMP REQUIRED gmp)

###################################
# pluto
###################################
ExternalProject_Add(
    pluto                # Name of the project
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/pluto
    CONFIGURE_COMMAND ${CMAKE_SOURCE_DIR}/pluto/configure --prefix=${CMAKE_BINARY_DIR}/pluto
    BUILD_COMMAND make -j
    INSTALL_COMMAND make install
)

###################################
# trahrhe
###################################

# then we can build it
add_custom_target(
  trahrhe
  COMMAND make
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/trahrhe-dev
)

###################################
# muparser
###################################
add_subdirectory(muparser)

###################################
# libpesto
###################################

# Sources files
file(GLOB_RECURSE LIBPESTO_SOURCES 
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.cpp" 
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.c"
)

# Add libraries
add_library(pesto STATIC ${LIBPESTO_SOURCES})

add_dependencies(pesto pluto)
target_link_libraries(pesto PRIVATE ${GMP_LIBRARIES})

# pluto dependency
target_link_libraries(pesto PRIVATE ${CMAKE_BINARY_DIR}/pluto/lib/libpluto.a)
target_include_directories(pesto PRIVATE ${CMAKE_BINARY_DIR}/pluto/include)

# isl dependency
target_link_libraries(pesto PRIVATE ${CMAKE_BINARY_DIR}/pluto/lib/libisl.a)

# osl dependency
target_link_libraries(pesto PRIVATE ${CMAKE_BINARY_DIR}/pluto/lib/libosl.a)

# cloog dependency
target_link_libraries(pesto PRIVATE ${CMAKE_BINARY_DIR}/pluto/lib/libcloog-isl.a)

# candl dependency
target_link_libraries(pesto PRIVATE ${CMAKE_BINARY_DIR}/pluto/lib/libcandl.a)

# piplib dependency
target_link_libraries(pesto PRIVATE ${CMAKE_BINARY_DIR}/pluto/lib/libpiplib_dp.a)

# muparser dependency
target_link_libraries(pesto PRIVATE muparser)
target_include_directories(pesto PRIVATE ${muparser_INCLUDE_DIRS})

###################################
# pestotool
###################################

# Sources files
file(GLOB_RECURSE PESTOTOOL_SOURCES 
  "${CMAKE_CURRENT_SOURCE_DIR}/tool/*.cpp" 
  "${CMAKE_CURRENT_SOURCE_DIR}/tool/*.c"
)

# Add executables
add_executable(pestotool ${PESTOTOOL_SOURCES})

# trahrhe dependency
add_dependencies(pestotool trahrhe)

# libpesto dependency
add_dependencies(pestotool pesto)
target_link_libraries(pestotool PRIVATE pesto)

# pluto include
target_include_directories(pestotool PRIVATE ${CMAKE_BINARY_DIR}/pluto/include)

# clan dependency
target_link_libraries(pestotool PRIVATE ${CMAKE_BINARY_DIR}/pluto/lib/libclan.a)

# cloog dependency
target_link_libraries(pestotool PRIVATE ${CMAKE_BINARY_DIR}/pluto/lib/libcloog-isl.a)

# muparser dependency
target_link_libraries(pestotool PRIVATE muparser)
target_include_directories(pestotool PRIVATE ${muparser_INCLUDE_DIRS})

###################################
# pesto-cli
###################################

# Define variable for the templates
set(EXTRACT_SCOP_BIN "${CMAKE_BINARY_DIR}/extract_scop.sh")
set(REPLACE_SCOP_BIN "${CMAKE_BINARY_DIR}/replace_scop.sh")
set(TRAHRHE_WRAPPER_BIN "${CMAKE_BINARY_DIR}/trahrhe_wrapper.sh")
set(PESTO_TOOL_BIN "${CMAKE_BINARY_DIR}/pestotool")
set(TRAHRHE_INSTALL_DIR "${CMAKE_SOURCE_DIR}/trahrhe-dev")

# Add bash scripts
configure_file("${CMAKE_SOURCE_DIR}/extract_scop.sh" "${CMAKE_BINARY_DIR}" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/replace_scop.sh" "${CMAKE_BINARY_DIR}" COPYONLY)

configure_file(
  "${CMAKE_SOURCE_DIR}/pesto.in.sh" # input template file
  "${CMAKE_BINARY_DIR}/pesto"       # output file
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  @ONLY
)
configure_file(
  "${CMAKE_SOURCE_DIR}/trahrhe_wrapper.in.sh" # input template file
  "${CMAKE_BINARY_DIR}/trahrhe_wrapper.sh"    # output file
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  @ONLY
)

# TODO
# Add tests
# if(BUILD_TESTING)
#   enable_testing()
#   add_subdirectory(tests)
# endif()

# TODO
# Documentation
# if(DOXYGEN_FOUND)
#   configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
#   add_custom_target(doc
#     ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
#     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
#     COMMENT "Generating API documentation with Doxygen" VERBATIM
#   )
# endif()

# Install targets
install(TARGETS pesto pestotool
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
# Install headers
install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ 
  DESTINATION include 
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
# Install trahrhe
install(
  DIRECTORY ${CMAKE_BINARY_DIR}/trahrhe/ 
  DESTINATION trahrhe
)
# install scripts
install(
  PROGRAMS ${CMAKE_BINARY_DIR}/pesto ${CMAKE_BINARY_DIR}/trahrhe_wrapper.sh ${CMAKE_BINARY_DIR}/extract_scop.sh ${CMAKE_BINARY_DIR}/replace_scop.sh
  DESTINATION bin
)

# Print configuration summary
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Linker flags: ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")