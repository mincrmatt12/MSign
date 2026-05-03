# Builds the STM binaries
cmake_minimum_required(VERSION 3.13)
project(msign-pwr C CXX ASM)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
	message(STATUS "Release build, using LTO")
endif()
set(CMAKE_CXX_STANDARD 23)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   add_compile_options (-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
   add_compile_options (-fcolor-diagnostics)
endif ()

# specify target
set(MSIGN_BUILD_TYPE "ngboard" CACHE STRING "Build target, expected to be ngboard since other target don't have a pwr processor")
set(MSIGN_OCD_INTERFACE "cmsis-dap" CACHE STRING "Interface type to use with OpenOCD")

# setup cpu name & clock speed
if (MSIGN_BUILD_TYPE STREQUAL "ngboard")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m0plus")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-m0plus")

	add_definitions(
		-DF_CPU=16000000L
	)
else()
	message(FATAL_ERROR "Unsupported target")
endif()

# Check if we can find an openocd binary

find_program(openocd_executable openocd)
find_package(Python 3 COMPONENTS Interpreter)

# Add libraries for the vendored libs

set(cube_ll_drivers
	gpio.c
	pwr.c
	rcc.c
	i2c.c
	ucpd.c
)

# hal
list(TRANSFORM cube_ll_drivers PREPEND ${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32G0xx_HAL_Driver/Src/stm32g0xx_ll_ OUTPUT_VARIABLE cube_ll_srcs)

add_library(cube_ll OBJECT ${cube_ll_srcs})

target_include_directories(cube_ll PUBLIC
	${CMAKE_CURRENT_LIST_DIR}/../vendor/CMSIS_6/CMSIS/Core/Include)

target_include_directories(cube_ll PUBLIC
	${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32G0xx_HAL_Driver/Inc
	${CMAKE_CURRENT_LIST_DIR}/../vendor/CMSIS_Device_STM32G0xx/Include
)

#target_include_directories(cube_ll PUBLIC
#${CMAKE_CURRENT_SOURCE_DIR}/cfg
#)

target_compile_definitions(cube_ll PUBLIC
	USE_FULL_LL_DRIVER
	STM32G071xx
)

target_compile_options(cube_ll PUBLIC
	$<$<COMPILE_LANGUAGE:CXX>: -fno-threadsafe-statics -Wno-register -Wno-volatile>
)

set(freertos_dir ${CMAKE_CURRENT_LIST_DIR}/../vendor/FreeRTOS)

# rtos
# compiled against cm3 because we don't use the cm4 fpu
add_library(freertos OBJECT
	${freertos_dir}/event_groups.c
	${freertos_dir}/list.c
	${freertos_dir}/queue.c
	${freertos_dir}/stream_buffer.c
	${freertos_dir}/tasks.c

	${freertos_dir}/portable/GCC/ARM_CM0/port.c
)

target_include_directories(freertos PUBLIC
	${freertos_dir}/include
	${freertos_dir}/portable/GCC/ARM_CM0

	${CMAKE_CURRENT_SOURCE_DIR}/cfg
)

# get the actual app sources
file(GLOB_RECURSE stm_srcs FOLLOW_SYMLINKS 
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.c 
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.s
)

file(GLOB stm_libc_srcs
	${CMAKE_CURRENT_SOURCE_DIR}/../stm/src/libc/*.c
)

list(REMOVE_ITEM stm_srcs ${stm_libc_srcs})
list(REMOVE_ITEM stm_libc_srcs ${CMAKE_CURRENT_SOURCE_DIR}/../stm/src/libc/printf.c)

add_library(minlibc STATIC ${stm_libc_srcs})
add_library(minlibc_printf OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/../stm/src/libc/printf.c)

target_link_libraries(minlibc gcc minlibc_printf)
# ensure definitions of libc methods don't try to use themselves recursively
target_compile_options(minlibc PRIVATE -fno-builtin)
set_property(TARGET minlibc PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)

target_include_directories(minlibc SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/libc/include)

add_executable(pwr ${stm_srcs})

target_link_libraries(pwr
	cube_ll
	freertos

	minlibc
)

target_compile_options(pwr
	PRIVATE
	-O3
	-ffast-math
)

target_compile_options(pwr
	PUBLIC
	-fexec-charset=cp1252
)

# Get the current version
if (${Python_FOUND})
	execute_process(COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/../stm/get_ver.py
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
		OUTPUT_VARIABLE version_str
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	message(STATUS "Revision code is ${version_str}")

	target_compile_definitions(pwr PUBLIC
		MSIGN_GIT_REV=\"${version_str}\"
	)
endif()

execute_process(COMMAND ${CMAKE_LINKER} --help OUTPUT_VARIABLE MSIGN_LINKER_HELP)
if ("${MSIGN_LINKER_HELP}" MATCHES ".*warn-rwx.*")
	set(USE_NO_WARN_RWX 1)
endif()

target_link_options(pwr
	PUBLIC
	-T${CMAKE_CURRENT_SOURCE_DIR}/ld/g0.ld
	-Wl,--undefined=uxTopUsedPriority
	-Wl,--gc-sections,--relax
	-Wl,--nmagic
	-Wl,-Map=pwr.map
	$<$<BOOL:${USE_NO_WARN_RWX}>:-Wl,--no-warn-rwx-segment>
)

if (EXISTS ${openocd_executable})
	message(STATUS "Found OPENOCD " ${openocd_executable})

	# add upload target
	add_custom_target(upload
		COMMAND ${openocd_executable} -f interface/${MSIGN_OCD_INTERFACE}.cfg -f target/stm32g0x.cfg -c "program $<TARGET_FILE:pwr> verify reset exit" || echo "program $<TARGET_FILE:pwr> verify reset" | telnet localhost 4444
	)

	add_dependencies(upload pwr)
else()
	message(STATUS "Disabling upload target due to no OpenOCD")
endif()

add_custom_target(pwr_bin ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/pwr.bin)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pwr.bin 
	COMMAND ${OBJCOPY_EXECUTABLE} -Obinary $<TARGET_FILE:pwr> ${CMAKE_CURRENT_BINARY_DIR}/pwr.bin
	DEPENDS pwr)
