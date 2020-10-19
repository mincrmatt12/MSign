# Builds the STM binaries
cmake_minimum_required(VERSION 3.13)
project(msign-stm C CXX ASM)

set(CMAKE_EXE_LINKER_FLAGS "")
set(CMAKE_CXX_STANDARD 17)

# specify target
set(MSIGN_BUILD_TYPE "board" CACHE STRING "Build target, either 'nucleo' or 'board'")

# Check if we can find an openocd binary

find_program(openocd_executable
	openocd
	PATHS $ENV{HOME}/.platformio/packages/tool-openocd/bin ENV OPENOCD_DIR
	NO_DEFAULT_PATH
	REQUIRED
)

message(STATUS "Found OPENOCD " ${openocd_executable})

find_package(Python 3 COMPONENTS Interpreter)

# Add libraries for the vendored libs

# hal
file(GLOB cube_ll_srcs ${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF2/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_ll_*.c)

add_library(cube_ll STATIC ${cube_ll_srcs})

target_include_directories(cube_ll PUBLIC
	${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF2/Drivers/STM32F2xx_HAL_Driver/Inc
	${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF2/Drivers/CMSIS/Device/ST/STM32F2xx/Include
	${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF2/Drivers/CMSIS/Include
	${CMAKE_CURRENT_SOURCE_DIR}/cfg
)

target_compile_definitions(cube_ll PUBLIC
	USE_FULL_LL_DRIVER
)

if (MSIGN_BUILD_TYPE STREQUAL "board")
	target_compile_definitions(cube_ll PUBLIC
		-DSTM32F205xx
		-DF_CPU=120000000L
	)
elseif (MSIGN_BUILD_TYPE STREQUAL "nucleo")
	target_compile_definitions(cube_ll PUBLIC
		-DSTM32F207xx
		-DF_CPU=120000000L
	)
endif()


set(freertos_dir ${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF2/Middlewares/Third_Party/FreeRTOS/Source)

# rtos
add_library(freertos STATIC
	${freertos_dir}/event_groups.c
	${freertos_dir}/list.c
	${freertos_dir}/queue.c
	${freertos_dir}/stream_buffer.c
	${freertos_dir}/tasks.c

	${freertos_dir}/portable/GCC/ARM_CM3/port.c
	${freertos_dir}/portable/MemMang/heap_1.c # we never malloc on the sign from the stm
)

target_include_directories(freertos PUBLIC
	${freertos_dir}/include
	${freertos_dir}/portable/GCC/ARM_CM3

	${CMAKE_CURRENT_SOURCE_DIR}/cfg
)

# get the actual app sources
file(GLOB_RECURSE stm_srcs FOLLOW_SYMLINKS 
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.c 
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.s
)

add_executable(stm ${stm_srcs})

target_link_libraries(stm
	cube_ll
	freertos
	c m gcc
)

target_compile_options(stm
	PUBLIC
	-Og
	-ffast-math
	$<$<COMPILE_LANGUAGE:CXX>:-Wno-pmf-conversions -fno-threadsafe-statics -Wno-register>
)

# Get the current version
if (${Python_FOUND})
	execute_process(COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/get_ver.py
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
		OUTPUT_VARIABLE version_str
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	message(STATUS "Revision code is ${version_str}")

	target_compile_definitions(stm PUBLIC
		MSIGN_GIT_REV=\"${version_str}\"
	)
endif()

target_link_options(stm
	PUBLIC
	-Wl,-T${CMAKE_CURRENT_SOURCE_DIR}/ld/f207zg.ld
	-Wl,--undefined=uxTopUsedPriority
	-Wl,--gc-sections,--relax
	-Wl,--nmagic
	-Wl,-u 
	-Wl,_printf_float
	--specs=nano.specs
)

# add upload target
add_custom_target(upload
	COMMAND ${openocd_executable} -f interface/stlink.cfg -f target/stm32f2x.cfg -c "program $<TARGET_FILE:stm> verify reset exit"
)

add_dependencies(upload stm)

add_custom_target(stm.bin ALL DEPENDS stm COMMAND ${OBJCOPY_EXECUTABLE} -Obinary $<TARGET_FILE:stm> ${CMAKE_CURRENT_BINARY_DIR}/stm.bin)
