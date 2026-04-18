# ##################################################################################################
# pestobin
# ##################################################################################################

# Sources files
set(PESTOBIN_SOURCES
    src/main.cpp
    src/Options.cpp
    src/utils/Process.cpp
)

# Add executable
add_executable(pesto-cli ${PESTOBIN_SOURCES})
add_dependencies(pesto-cli pesto clan)

# Add include directories
target_include_directories(pesto-cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# libpesto dependency
target_link_libraries(pesto-cli clan pesto)

set_target_properties(pesto-cli PROPERTIES OUTPUT_NAME "pesto")
