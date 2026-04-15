# muparser
message(STATUS "Fetching muparser")

FetchContent_Declare(
    muparser #
    DOWNLOAD_EXTRACT_TIMESTAMP true #
    EXCLUDE_FROM_ALL
    URL https://github.com/beltoforion/muparser/archive/refs/tags/v2.3.5.zip
)
set(ENABLE_SAMPLES
    off
    CACHE BOOL "" FORCE
)
set(BUILD_SHARED_LIBS
    off
    CACHE BOOL "" FORCE
)
set(BUILD_TESTING
    off
    CACHE BOOL "" FORCE
)

dump_cmake_variables("muparser.*")
if(CMAKE_VERSION VERSION_LESS "3.28")
    fetchcontent_getproperties(muparser)
    if(NOT muparser_POPULATED)
        fetchcontent_populate(muparser)
        add_subdirectory(${muparser_SOURCE_DIR} ${muparser_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
else()
    fetchcontent_makeavailable(muparser)
endif()
