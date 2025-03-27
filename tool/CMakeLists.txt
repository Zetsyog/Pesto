# ##################################################################################################
# pestobin
# ##################################################################################################

# Sources files
set(PESTOTOOL_SOURCES main.cpp Options.cpp)

# Add executable
add_executable(pestobin ${PESTOTOOL_SOURCES})

# libpesto dependency
target_link_libraries(pestobin PRIVATE pesto clan)

# Add include directories
target_include_directories(pestobin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
