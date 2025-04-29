# FindNTL.cmake - A script to locate the NTL library and its headers

find_package(PkgConfig REQUIRED)

# GMP is a required dependency for NTL
pkg_check_modules(GMP REQUIRED gmp)

# Search for the NTL library
find_library(
  NTL_LIBRARY
  NAMES ntl
  PATHS /usr/lib /usr/local/lib /opt/lib /nix/store
  PATH_SUFFIXES lib)

# Search for the NTL include directory
find_path(
  NTL_INCLUDE_DIR
  NAMES NTL/ZZ.h
  PATHS /usr/include /usr/local/include /opt/include /nix/store
  PATH_SUFFIXES include)

# Check if both the library and include directory were found
if(NTL_LIBRARY AND NTL_INCLUDE_DIR)
  set(NTL_FOUND TRUE)
  set(NTL_LIBRARIES ${NTL_LIBRARY})
  set(NTL_INCLUDE_DIRS ${NTL_INCLUDE_DIR})
  message(STATUS "Found NTL library: ${NTL_LIBRARY}")
  message(STATUS "Found NTL include directory: ${NTL_INCLUDE_DIR}")
else()
  set(NTL_FOUND FALSE)
  message(
    WARNING "NTL library not found. Please install it or provide a custom path."
  )
endif()

# Mark the variables as advanced to hide them in GUIs
mark_as_advanced(NTL_LIBRARY NTL_INCLUDE_DIR)
