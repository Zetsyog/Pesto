# require isl
message(STATUS "Checking cloog-isl")

# Cloog patch files
set(CLOOG_PATCH_CLAST_USER_VEC
	${CMAKE_SOURCE_DIR}/patches/cloog-Enhanced-CLAST_PARALLEL_USER-directive.patch
)
set(CLOOG_PATCH_CONFIGURE_FIX
	${CMAKE_SOURCE_DIR}/patches/cloog-configure.ac-fix-TEXI2DVI-check-to-handle-missing-pr.patch
)

include(ExternalProject)

set(CLOOG_BIN ${CMAKE_CURRENT_BINARY_DIR}/cloog)
set(CLOOG_STATIC_LIB ${CLOOG_BIN}/lib/libcloog-isl.a)
set(CLOOG_INCLUDES ${CLOOG_BIN}/include)

ExternalProject_Add(
	cloog-isl
	GIT_REPOSITORY https://github.com/periscop/cloog.git
	GIT_TAG cloog-0.21.1 #
	GIT_SUBMODULES_RECURSE false
	PREFIX ${CLOOG_BIN}
	BUILD_IN_SOURCE true
	CONFIGURE_COMMAND
		./autogen.sh && ./configure --prefix=${CLOOG_BIN} #
		--with-isl=system --with-isl-prefix=${PLUTO_BIN} #
		--with-osl=system --with-osl-prefix=${PLUTO_BIN}
	PATCH_COMMAND
		${CMAKE_COMMAND} -E echo "Applying ${CLOOG_PATCH_CLAST_USER_VEC}" && #
		patch -p1 < ${CLOOG_PATCH_CLAST_USER_VEC} && #
		${CMAKE_COMMAND} -E echo "Applying ${CLOOG_PATCH_CONFIGURE_FIX}" && #
		patch -p1 < ${CLOOG_PATCH_CONFIGURE_FIX}
	BUILD_COMMAND make -j${PESTO_BUILD_JOBS}
	INSTALL_COMMAND make install
	UPDATE_DISCONNECTED true
)

add_library(cloog STATIC IMPORTED GLOBAL)
add_dependencies(cloog cloog-isl)
add_dependencies(cloog isl)
add_dependencies(cloog osl)
set_target_properties(cloog PROPERTIES IMPORTED_LOCATION ${CLOOG_STATIC_LIB})
set_target_properties(cloog PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CLOOG_INCLUDES})
target_link_libraries(cloog INTERFACE isl osl)
