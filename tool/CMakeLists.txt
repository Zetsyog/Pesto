# ##################################################################################################
# pestobin
# ##################################################################################################

# Sources files
set(PESTOBIN_SOURCES main.cpp Options.cpp)

# Add executable
add_executable(pestobin ${PESTOBIN_SOURCES})
add_dependencies(pestobin pesto clan)

# libpesto dependency
target_link_libraries(pestobin clan pesto)

# Add include directories
target_include_directories(pestobin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
