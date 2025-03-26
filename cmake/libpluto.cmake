# require isl
message(STATUS "Checking libpluto")

include(ExternalProject)

set(PLUTO_BIN ${CMAKE_CURRENT_BINARY_DIR}/pluto)
set(PLUTO_STATIC_LIB ${PLUTO_BIN}/lib/libpluto.a)
set(PLUTO_INCLUDES ${PLUTO_BIN}/include)

file(MAKE_DIRECTORY ${PLUTO_INCLUDES})

pkg_check_modules(GMP REQUIRED gmp)

# Pluto patch files
set(PLUTO_PATCH_COMPILE_FIX ${CMAKE_SOURCE_DIR}/patches/pluto_doc_gen_fix.patch)

ExternalProject_Add(
	libpluto
	GIT_REPOSITORY https://gitlab.inria.fr/crossett/pluto-openscop.git
	GIT_TAG origin/libpluto-build #
	GIT_SUBMODULES_RECURSE false
	PREFIX ${PLUTO_BIN}
	BUILD_IN_SOURCE true
	CONFIGURE_COMMAND ./autogen.sh && ./configure --prefix=${PLUTO_BIN} --enable-libonly
	PATCH_COMMAND ${CMAKE_COMMAND} -E echo "Applying ${PLUTO_PATCH_COMPILE_FIX}" && #
				  patch -p1 < ${PLUTO_PATCH_COMPILE_FIX}
	BUILD_COMMAND make -j${PESTO_BUILD_JOBS}
	INSTALL_COMMAND make install
	UPDATE_DISCONNECTED true
	BUILD_BYPRODUCTS ${PLUTO_STATIC_LIB}
)

add_library(pluto STATIC IMPORTED GLOBAL)
add_dependencies(pluto libpluto)
set_target_properties(pluto PROPERTIES IMPORTED_LOCATION ${PLUTO_STATIC_LIB})
set_target_properties(pluto PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${PLUTO_INCLUDES})

add_library(osl STATIC IMPORTED GLOBAL)
add_dependencies(osl pluto)
set_target_properties(osl PROPERTIES IMPORTED_LOCATION ${PLUTO_BIN}/lib/libosl.a)
set_target_properties(osl PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${PLUTO_BIN}/include)

add_library(isl STATIC IMPORTED GLOBAL)
add_dependencies(isl pluto)
set_target_properties(isl PROPERTIES IMPORTED_LOCATION ${PLUTO_BIN}/lib/libisl.a)
set_target_properties(isl PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${PLUTO_BIN}/include)

add_library(candl STATIC IMPORTED GLOBAL)
add_dependencies(candl pluto)
set_target_properties(candl PROPERTIES IMPORTED_LOCATION ${PLUTO_BIN}/lib/libcandl.a)
set_target_properties(candl PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${PLUTO_BIN}/include)

add_library(piplib STATIC IMPORTED GLOBAL)
add_dependencies(piplib pluto)
set_target_properties(piplib PROPERTIES IMPORTED_LOCATION ${PLUTO_BIN}/lib/libpiplib_dp.a)
set_target_properties(piplib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${PLUTO_BIN}/include)

target_link_libraries(pluto INTERFACE isl osl candl piplib ${GMP_LIBRARIES})
