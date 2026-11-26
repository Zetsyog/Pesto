#
# Pesto - A polyhedral loop nest optimizer
#
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2024 Clément Rossetti
#


# ##################################################################################################
# libpesto
# ##################################################################################################

set(PESTO_SOURCES
    src/ParamConstraint.cpp
    src/PassManager.cpp
    src/PlutoWrapper.cpp
    src/atile/TpzMaxCut.cpp
    src/byproduct/HeaderFile.cpp
    src/byproduct/OutputSourceFile.cpp
    src/byproduct/SideSourceFile.cpp
    src/codegen/ParamTiling.cpp
    src/ir/OpenScopIR.cpp
    src/ir/ClastIR.cpp
    src/ir/Isl2Scop.cpp
    src/ir/Scop2Isl.cpp
    src/clast_support.c
    src/isl_support.c
    src/osl_support.c
    src/logging/Logging.cpp
    src/pass/CloogCodegen.cpp
    src/pass/CustomVecPragma.cpp
    src/pass/DumpOsl.cpp
    src/pass/GenClast.cpp
    src/pass/IslTest.cpp
    src/pass/MatchCriteria.cpp
    src/pass/ParamContext.cpp
    src/pass/ParamTiling.cpp
    src/pass/Pass.cpp
    src/pass/Pluto.cpp
    src/pass/TpzTiling.cpp
    src/pass/SkewTransform.cpp
    src/pass/SkewPass.cpp
)

add_library(pesto STATIC ${PESTO_SOURCES})

set(PESTO_TEMPLATE_SCHEDULE ${CMAKE_CURRENT_BINARY_DIR}/template/schedule.h)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in # input template file
    ${PESTO_BINARY_HEADER_DIR}/pesto/config.h # output file
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/export/schedule.h # input template file
    ${PESTO_TEMPLATE_SCHEDULE} # output file
    COPYONLY # do not configure
)

add_dependencies(pesto trahrhe-src barvinok-src)

target_include_directories(
    pesto PUBLIC $<BUILD_INTERFACE:${PESTO_SOURCE_HEADER_DIR}>
    $<BUILD_INTERFACE:${PESTO_BINARY_HEADER_DIR}> $<INSTALL_INTERFACE:include>
)
target_include_directories(pesto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(pesto PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(
    pesto PUBLIC
    muparser
    spdlog
    pluto
    cloog
    osl
    trahrhe
    ${GMP_LIBRARIES}
)
