include(ExternalProject)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GMP REQUIRED gmp)
find_package(NTL REQUIRED)

if(NOT NTL_FOUND)
	message(FATAL_ERROR "NTL library not found. Please install NTL.")
endif()

set(BARVINOK_VERSION 0.41.8)
set(BARVINOK_URL "https://barvinok.sourceforge.io/barvinok-${BARVINOK_VERSION}.tar.gz")
set(BARVINOK_INSTALL_DIR
	"${CMAKE_CURRENT_BINARY_DIR}/barvinok"
	CACHE INTERNAL "Path to Barvinok installation directory"
)

# Locate the Barvinok library
ExternalProject_Add(
	barvinok-src
	URL ${BARVINOK_URL} DOWNLOAD_EXTRACT_TIMESTAMP true
	PREFIX ${CMAKE_CURRENT_BINARY_DIR}/barvinok
	BUILD_IN_SOURCE true
	CONFIGURE_COMMAND
		./configure --prefix=${BARVINOK_INSTALL_DIR} #
		--with-isl=build --with-isl-builddir=${PLUTO_SRC_DIR}/isl/ #
		CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} LD=${CMAKE_LINKER}
	BUILD_COMMAND make
	INSTALL_COMMAND make install
)

link_directories(${BARVINOK_INSTALL_DIR}/lib)

set(BARVINOK_LIBRARIES barvinok ${NTL_LIBRARIES} ${GMP_LIBRARIES} polylibgmp)

add_dependencies(barvinok-src pluto-src)
