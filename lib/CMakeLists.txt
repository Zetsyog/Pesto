# ##################################################################################################
# libpesto
# ##################################################################################################

set(PESTO_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/OslScop.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ParamTilingUtility.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/PassManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/PestoProgram.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/PlutoWrapper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/clast/CustomVecPragma.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/clast/GenClastPass.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/clast/ParamTilingPass.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/clast/clast_support.c
	${CMAKE_CURRENT_SOURCE_DIR}/codegen/CloogPass.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/codegen/VarDecls.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/codegen/ParamTiling.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/isl_support.c
	${CMAKE_CURRENT_SOURCE_DIR}/osl_support.c
	${CMAKE_CURRENT_SOURCE_DIR}/pass/DumpOslPass.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/pass/MatchCriteriaPass.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/pass/ParamContextPass.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/pass/Pass.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/pass/PlutoPass.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/pass/SplitNestPass.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/passbuilder/ClastCustomVecPragmaPB.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/passbuilder/ClastParamTilingPB.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/passbuilder/CloogCodegenPB.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/passbuilder/GenClastPB.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/passbuilder/MatchCriteriaPB.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/passbuilder/PMBuilder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/passbuilder/ParamContextPB.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/passbuilder/PassBuilder.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/passbuilder/PlutoPB.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/passbuilder/SplitNestPB.cpp
)

add_library(pesto STATIC ${PESTO_SOURCES})

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/version.h.in # input template file
	${PESTO_BINARY_HEADER_DIR}/pesto/version.h # output file
)

set(PESTO_TEMPLATE_SCHEDULE ${CMAKE_CURRENT_BINARY_DIR}/template/schedule.h)
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/export/schedule.h # input template file
	${PESTO_TEMPLATE_SCHEDULE} # output file
	COPYONLY # do not configure
)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/codegen/ParamTiling.hpp.in # input file
	${PESTO_BINARY_HEADER_DIR}/pesto/codegen/ParamTiling.hpp # output file
)

add_dependencies(pesto trahrhe-src barvinok-src)

target_include_directories(
	pesto PUBLIC $<BUILD_INTERFACE:${PESTO_SOURCE_HEADER_DIR}>
				 $<BUILD_INTERFACE:${PESTO_BINARY_HEADER_DIR}> $<INSTALL_INTERFACE:include>
)
target_include_directories(pesto PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/include)
target_include_directories(pesto PUBLIC ${TRAHRHE_INCLUDE_DIRS})
target_include_directories(pesto PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pesto PUBLIC pluto cloog nlohmann_json muparser ${TRAHRHE_LIBRARIES})

message(STATUS "Trahrhe libraries: ${TRAHRHE_LIBRARIES}")
