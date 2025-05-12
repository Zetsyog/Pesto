# require isl
message(STATUS "Checking libpluto")

include(ExternalProject)

set(PLUTO_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/pluto)
set(PLUTO_STATIC_LIB ${PLUTO_PREFIX}/lib/libpluto.a)
set(PLUTO_INCLUDES ${PLUTO_PREFIX}/include)
set(PLUTO_SRC_DIR ${PLUTO_PREFIX}/src/pluto-src)

file(MAKE_DIRECTORY ${PLUTO_INCLUDES})

pkg_check_modules(GMP REQUIRED gmp)

# Pluto patch files
set(PLUTO_PATCH_COMPILE_FIX ${CMAKE_SOURCE_DIR}/patches/pluto_doc_gen_fix.patch)
set(PLUTO_PATCH_FUNC_DEF_FIX
	${CMAKE_SOURCE_DIR}/patches/pluto-fix_multiple_definition_of_function.patch
)
ExternalProject_Add(
	pluto-src
	GIT_REPOSITORY https://gitlab.inria.fr/crossett/pluto-openscop.git
	GIT_TAG 1f6eed199b38526fc093b76638e50c6a3e12faed #
	GIT_SUBMODULES_RECURSE false
	GIT_SUBMODULES isl openscop piplib candl #
	PREFIX ${PLUTO_PREFIX}
	BUILD_IN_SOURCE true
	CONFIGURE_COMMAND ./autogen.sh && ./configure --prefix=${PLUTO_PREFIX} --disable-polycc
	PATCH_COMMAND
		${CMAKE_COMMAND} -E echo "Applying ${PLUTO_PATCH_COMPILE_FIX}" && #
		patch -p1 < ${PLUTO_PATCH_COMPILE_FIX} && #
		patch -p1 < ${PLUTO_PATCH_FUNC_DEF_FIX} #
	BUILD_COMMAND make -j${PESTO_BUILD_JOBS}
	INSTALL_COMMAND make install
	UPDATE_DISCONNECTED true
	BUILD_BYPRODUCTS ${PLUTO_STATIC_LIB}
)

add_library(pluto STATIC IMPORTED GLOBAL)
add_dependencies(pluto pluto-src)
set_target_properties(pluto PROPERTIES IMPORTED_LOCATION ${PLUTO_STATIC_LIB})
set_target_properties(pluto PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${PLUTO_INCLUDES})

add_library(osl STATIC IMPORTED GLOBAL)
add_dependencies(osl pluto)
set_target_properties(osl PROPERTIES IMPORTED_LOCATION ${PLUTO_PREFIX}/lib/libosl.a)
set_target_properties(osl PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${PLUTO_PREFIX}/include)

add_library(isl STATIC IMPORTED GLOBAL)
add_dependencies(isl pluto)
set_target_properties(isl PROPERTIES IMPORTED_LOCATION ${PLUTO_PREFIX}/lib/libisl.a)
set_target_properties(isl PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${PLUTO_PREFIX}/include)

add_library(candl STATIC IMPORTED GLOBAL)
add_dependencies(candl pluto)
set_target_properties(candl PROPERTIES IMPORTED_LOCATION ${PLUTO_PREFIX}/lib/libcandl.a)
set_target_properties(candl PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${PLUTO_PREFIX}/include)

add_library(piplib STATIC IMPORTED GLOBAL)
add_dependencies(piplib pluto)
set_target_properties(piplib PROPERTIES IMPORTED_LOCATION ${PLUTO_PREFIX}/lib/libpiplib_dp.a)
set_target_properties(piplib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${PLUTO_PREFIX}/include)

target_link_libraries(pluto INTERFACE isl osl candl piplib ${GMP_LIBRARIES})
