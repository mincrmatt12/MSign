cmake_minimum_required(VERSION 3.7)
project(testfnt)

set(CMAKE_CXX_STANDARD 17)

set(TESTFNT_FONT_FILE "" CACHE FILEPATH "path to font file")
set(TESTFNT_FONT_SIZE "" CACHE STRING "font size in pixels")

if("${TESTFNT_FONT_FILE}" STREQUAL "" OR "${TESTFNT_FONT_SIZE}" STREQUAL "")
	message(FATAL_ERROR "set font file and pixel size")
endif()

message(STATUS "Using font file: ${TESTFNT_FONT_FILE}")
message(STATUS "Using font size: ${TESTFNT_FONT_SIZE}")

set(FONT_FILE_PATH ${CMAKE_CURRENT_BINARY_DIR}/genfont)

add_custom_command(OUTPUT ${FONT_FILE_PATH}.h ${FONT_FILE_PATH}.cpp
	COMMAND ${CMAKE_CURRENT_LIST_DIR}/../fnter.py ${TESTFNT_FONT_FILE} ${TESTFNT_FONT_SIZE} > ${FONT_FILE_PATH}.cpp
	COMMAND ${CMAKE_CURRENT_LIST_DIR}/../fnter.py ${TESTFNT_FONT_FILE} h${TESTFNT_FONT_SIZE} > ${FONT_FILE_PATH}.h
	DEPENDS ${TESTFNT_FONT_FILE} ${CMAKE_CURRENT_LIST_DIR}/../fnter.py
	COMMENT "Generating font data...."
)

add_custom_target(generate_font_data DEPENDS ${FONT_FILE_PATH}.h ${FONT_FILE_PATH}.cpp)

execute_process(COMMAND python3 ${CMAKE_CURRENT_LIST_DIR}/cmake/getfontname.py ${TESTFNT_FONT_FILE} ${TESTFNT_FONT_SIZE}
	OUTPUT_VARIABLE TESTFNT_FONT_SNAME
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Using font name: ${TESTFNT_FONT_SNAME}")

add_executable(testfnt testfnt.cpp ${FONT_FILE_PATH}.cpp)
add_dependencies(testfnt generate_font_data)
target_compile_definitions(testfnt PRIVATE TESTFNT_FONT=${TESTFNT_FONT_SNAME})
target_include_directories(testfnt PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
