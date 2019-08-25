cmake_minimum_required(VERSION 3.7)
project(signsim)

set(CMAKE_CXX_STANDARD 17)

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
file(GLOB_RECURSE STM_OVERRIDE_SRCS src/stm/*.cpp)
file(GLOB_RECURSE STM_SRCS ${CMAKE_CURRENT_LIST_DIR}/../stm/src/*.cpp)

# now, create a new list, called TOTAL_STM_SRCS, from the original list
set(TOTAL_STM_SRCS ${STM_SRCS})
list(TRANSFORM TOTAL_STM_SRCS REPLACE "([\\w/.\d]+)/.../src" "stmsrc")
list(TRANSFORM STM_OVERRIDE_SRCS REPLACE "([\\w/.\d]+)/.../src" "stmsrc")

# concatenate the lists
list(APPEND TOTAL_STM_SRCS ${STM_OVERRIDE_SRCS})
list(REMOVE_DUPLICATES TOTAL_STM_SRCS)
list(TRANSFORM TOTAL_STM_SRCS PREPEND ${CMAKE_CURRENT_BINARY_DIR)

message(STATUS "Creating copy targets")

# c) create copy targets
add_custom_target(copy_stm_src)
add_custom_command(TARGET copy_stm_src PRE_BUILD
	COMMENT Copying original stm sources 
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_CURRENT_LIST_DIR}/../stm/src/ ${CMAKE_CURRENT_BINARY_DIR}/stmsrc
)
add_custom_command(TARGET copy_stm_src PRE_BUILD
	COMMENT Copying overridden stm sources 
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	src/stm/ ${CMAKE_CURRENT_BINARY_DIR}/stmsrc
)

add_custom_target(copy_esp_src)
add_custom_command(TARGET copy_esp_src PRE_BUILD
	COMMENT Copying original esp sources 
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_CURRENT_LIST_DIR}/../esp/src/ ${CMAKE_CURRENT_BINARY_DIR}/espsrc
)
add_custom_command(TARGET copy_esp_src PRE_BUILD
	COMMENT Copying overridden esp sources 
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	src/esp/ ${CMAKE_CURRENT_BINARY_DIR}/espsrc
)

# ----- section two: compile stm -----
add_executable(stmsim ${TOTAL_STM_SRCS})
target_include_directories(stmsim PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/stmsrc)
add_dependencies(stmsim copy_stm_src)

# ----- section three: setup ESP build -----

