# Check if trahrhe dependencies are installed
find_program(BARVINOK_ISCC "iscc" REQUIRED)
find_program(MAXIMA "maxima" REQUIRED)

add_custom_target(
	trahrhe
	COMMAND make
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/trahrhe-dev
)
