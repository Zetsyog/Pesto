# ##################################################################################################
# pestobin
# ##################################################################################################

# Sources files
set(PESTOBIN_SOURCES
    src/main.cpp
    src/Options.cpp
    src/utils/Process.cpp
    src/passbuilder/ClastCustomVecPragmaPB.cpp
    src/passbuilder/ClastParamTilingPB.cpp
    src/passbuilder/CloogCodegenPB.cpp
    src/passbuilder/DumpOsl.cpp
    src/passbuilder/GenClastPB.cpp
    src/passbuilder/IslPB.cpp
    src/passbuilder/MatchCriteriaPB.cpp
    src/passbuilder/PMBuilder.cpp
    src/passbuilder/ParamContextPB.cpp
    src/passbuilder/PassBuilder.cpp
    src/passbuilder/PlutoPB.cpp
    src/passbuilder/TpzTilingPB.cpp
)

# Add executable
add_executable(pesto-cli ${PESTOBIN_SOURCES})
add_dependencies(pesto-cli pesto clan)

# Add include directories
target_include_directories(pesto-cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# libpesto dependency
target_link_libraries(pesto-cli clan pesto nlohmann_json)

set_target_properties(pesto-cli PROPERTIES OUTPUT_NAME "pesto")
