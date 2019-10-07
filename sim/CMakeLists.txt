cmake_minimum_required(VERSION 3.12)
project(signsim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

# This CMake file is responsible for a) creating the source directories from:
# - the original directory
# - any overrides
#, b) compiling the STM codebase normally as a program and c) running the Arduino test runner to generate a file

# it then moves these files to the root directory for easy running

# ---- section one ---- creating source trees
# a) make directories
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/stmsrc)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/espsrc)

message(STATUS "Generating source trees...")

# b) glob for files
file(GLOB_RECURSE STM_OVERRIDE_SRCS ${CMAKE_CURRENT_LIST_DIR}/src/stm/*.c*)
file(GLOB_RECURSE STM_SRCS ${CMAKE_CURRENT_LIST_DIR}/../stm/src/*.c*)

file(GLOB_RECURSE ESP_OVERRIDE_SRCS ${CMAKE_CURRENT_LIST_DIR}/src/esp/*.c*)
file(GLOB_RECURSE ESP_SRCS ${CMAKE_CURRENT_LIST_DIR}/../esp/src/*.c*)

# now, create a new list, called TOTAL_STM_SRCS, from the original list
set(TOTAL_STM_SRCS ${STM_SRCS})
list(TRANSFORM TOTAL_STM_SRCS REPLACE "(.*)/.../src" "stmsrc")
list(TRANSFORM STM_OVERRIDE_SRCS REPLACE "(.*)/.../src/stm" "stmsrc")

# concatenate the lists
list(APPEND TOTAL_STM_SRCS ${STM_OVERRIDE_SRCS})
list(REMOVE_DUPLICATES TOTAL_STM_SRCS)
list(TRANSFORM TOTAL_STM_SRCS PREPEND ${CMAKE_CURRENT_BINARY_DIR}/)

# do the same for the ESP
set(TOTAL_ESP_SRCS ${ESP_SRCS})
list(TRANSFORM TOTAL_ESP_SRCS REPLACE "(.*)/.../src" "espsrc")
list(TRANSFORM ESP_OVERRIDE_SRCS REPLACE "(.*)/.../src/esp" "espsrc")

# concatenate the lists
list(APPEND TOTAL_ESP_SRCS ${ESP_OVERRIDE_SRCS})
list(REMOVE_DUPLICATES TOTAL_ESP_SRCS)
list(TRANSFORM TOTAL_ESP_SRCS PREPEND ${CMAKE_CURRENT_BINARY_DIR}/)

message(STATUS "Creating copy targets")

# c) create copy targets
add_custom_target(copy_stm_src)
add_custom_command(TARGET copy_stm_src PRE_BUILD
	COMMENT "Copying original stm sources" 
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_CURRENT_LIST_DIR}/../stm/src/ ${CMAKE_CURRENT_BINARY_DIR}/stmsrc
)
add_custom_command(TARGET copy_stm_src PRE_BUILD
	COMMENT "Copying overridden stm sources" 
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_CURRENT_LIST_DIR}/src/stm/ ${CMAKE_CURRENT_BINARY_DIR}/stmsrc
)

add_custom_target(copy_esp_src)
add_custom_command(TARGET copy_esp_src PRE_BUILD
	COMMENT "Copying original esp sources" 
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_CURRENT_LIST_DIR}/../esp/src/ ${CMAKE_CURRENT_BINARY_DIR}/espsrc
)
add_custom_command(TARGET copy_esp_src PRE_BUILD
	COMMENT "Copying overridden esp sources"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_CURRENT_LIST_DIR}/src/esp/ ${CMAKE_CURRENT_BINARY_DIR}/espsrc
)

# mark the files as generated
foreach(I IN LISTS TOTAL_STM_SRCS)
	set_source_files_properties(${I} PROPERTIES GENERATED TRUE)
endforeach()

foreach(I IN LISTS TOTAL_ESP_SRCS)
	set_source_files_properties(${I} PROPERTIES GENERATED TRUE)
endforeach()

# ----- section two: compile stm -----
add_executable(stmsim ${TOTAL_STM_SRCS})
target_include_directories(stmsim PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/stmsrc)
target_compile_options(stmsim PRIVATE -DSIM)
add_dependencies(stmsim copy_stm_src)

# ----- section three: setup ESP build -----
# A lot of this is stolen from https://github.com/esp8266/Arduino/blob/master/tests/host/Makefile

# a) find the ESPSim tree
set(ARDUINO_ESP_PATH "/home/matthew/ESPSimArduino" CACHE PATH "Path to a recent clone of https://github.com/esp8266/Arduino")
if (NOT IS_DIRECTORY ${ARDUINO_ESP_PATH})
	message(FATAL_ERROR Could not find ESP sources, please set them with -DARDUINO_ESP_PATH)
endif()

# a.2) set the list of libraries
set(ARDUINO_ESP_LIB_PATH "${CMAKE_CURRENT_LIST_DIR}/../esp/.pio/libdeps/msign_board" CACHE PATH "directory where pio libs are stored")
set(ARDUINO_ESP_LIBS "${ARDUINO_ESP_LIB_PATH}/Time" "${ARDUINO_ESP_LIB_PATH}/Timezone_ID76")
list(JOIN ARDUINO_ESP_LIBS ":" ARDUINO_ESP_LIBS_CMD)

# b) create external generation for the fullcore and bearssl static libs
# note: we check for makefiles so we can do proper parallelism
if ("${CMAKE_GENERATOR}" STREQUAL "Unix Makefiles")
	set(ARDUINO_MAKE_COMMAND "$(MAKE)")
else()
	set(ARDUINO_MAKE_COMMAND "make")
endif()

add_custom_command(OUTPUT "${ARDUINO_ESP_PATH}/tests/host/bin/fullcore.a"
	COMMAND ${CMAKE_COMMAND} -E env
		ULIBDIRS=${ARDUINO_ESP_LIBS_CMD}
		${ARDUINO_MAKE_COMMAND} bin/fullcore.a
	WORKING_DIRECTORY "${ARDUINO_ESP_PATH}/tests/host"
	COMMENT "Building ESP core library..."
)

add_custom_command(OUTPUT "${ARDUINO_ESP_PATH}/tools/sdk/ssl/bearssl/build/libbearssl.a"
	COMMAND ${CMAKE_COMMAND} -E env
		ULIBDIRS=${ARDUINO_ESP_LIBS_CMD}
		${ARDUINO_MAKE_COMMAND} ssl
	WORKING_DIRECTORY "${ARDUINO_ESP_PATH}/tests/host"
	COMMENT "Building BEARSSL library..."
)

# b.2) setup build targets
add_custom_target(esp_fullcore_lib_build DEPENDS "${ARDUINO_ESP_PATH}/tests/host/bin/fullcore.a")
add_custom_target(esp_bearssl_lib_build DEPENDS "${ARDUINO_ESP_PATH}/tools/sdk/ssl/bearssl/build/libbearssl.a")

# b.3) create 2 imported libraries
add_library(esp_bearssl STATIC IMPORTED GLOBAL)
set_target_properties(esp_bearssl
	PROPERTIES
	IMPORTED_LOCATION "${ARDUINO_ESP_PATH}/tools/sdk/ssl/bearssl/build/libbearssl.a"
)
add_dependencies(esp_bearssl esp_bearssl_lib_build)

# b.4) find all of the library include directories
file(GLOB ESP_CORE_LIBS_SRCS_INCLUDES "${ARDUINO_ESP_PATH}/libraries/*/src")
file(GLOB ESP_CORE_LIBS_INCLUDES "${ARDUINO_ESP_PATH}/libraries/*")

set(ARDUINO_ESP_LIB_INCLUDES ${ARDUINO_ESP_LIBS})
foreach(I IN LISTS ARDUINO_ESP_LIBS)
	if (IS_DIRECTORY ${I}/src)
		list(APPEND ARDUINO_ESP_LIB_INCLUDES ${I}/src)
	endif()
endforeach()

# b.5) create a linkable target for the esp8266 core libraries, with the right include dirs
add_library(esp_fullcore STATIC IMPORTED GLOBAL)
add_dependencies(esp_fullcore esp_fullcore_lib_build)
set_target_properties(esp_fullcore
	PROPERTIES 
	IMPORTED_LINK_INTERFACE_LIBRARIES
		esp_bearssl
	IMPORTED_LOCATION 
		"${ARDUINO_ESP_PATH}/tests/host/bin/fullcore.a"
)
target_include_directories(esp_fullcore INTERFACE
		"${ARDUINO_ESP_PATH}/tests/host"
		"${ARDUINO_ESP_PATH}/tests/host/common"
		"${ARDUINO_ESP_PATH}/cores/esp8266"
		${ARDUINO_ESP_LIB_INCLUDES}
		${ESP_CORE_LIBS_SRCS_INCLUDES}
		${ESP_CORE_LIBS_INCLUDES}
		"${ARDUINO_ESP_PATH}/tools/sdk/include"
		"${ARDUINO_ESP_PATH}/tools/sdk/lwip2/include"
)

target_compile_options(esp_fullcore INTERFACE
	"SHELL:-include ${ARDUINO_ESP_PATH}/tests/host/common/mock.h"
	"SHELL:-include ${ARDUINO_ESP_PATH}/tests/host/common/c_types.h"

	-DLWIP_IPV6=0
	-DHTTPCLIENT_1_1_COMPATIBLE=0
	-DHOST_MOCK=1
	-DNONOSDK221=1
	-DDEBUG_ESP_PORT=Serial1
	-Wimplicit-fallthrough=2
	-fno-rtti
)

# c) add target for ESP build
add_executable(espsim ${TOTAL_ESP_SRCS})
target_compile_options(espsim PRIVATE -DSIM)
target_link_libraries(espsim esp_fullcore)
target_include_directories(espsim PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/espsrc)
add_dependencies(espsim copy_esp_src)

# d) add target to run clean

add_custom_target(clean_arduino
	COMMAND ${ARDUINO_MAKE_COMMAND} clean
	WORKING_DIRECTORY "${ARDUINO_ESP_PATH}/tests/host"
)
