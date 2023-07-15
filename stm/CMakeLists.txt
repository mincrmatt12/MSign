# Builds the STM binaries
cmake_minimum_required(VERSION 3.13)
project(msign-stm C CXX ASM)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
	message(STATUS "Release build, using LTO")
endif()
set(CMAKE_CXX_STANDARD 20)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   add_compile_options (-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
   add_compile_options (-fcolor-diagnostics)
endif ()

# setup cpu name & clock speed
if (MSIGN_BUILD_TYPE STREQUAL "minisign")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m4")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-m4")

	add_definitions(
		-DF_CPU=168000000L
	)
else()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m3")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-m3")

	add_definitions(
		-DF_CPU=120000000L
	)
endif()

# specify target
set(MSIGN_BUILD_TYPE "board" CACHE STRING "Build target, either 'nucleo' or 'board' or 'minisign' (for f4)")
set(MSIGN_OCD_INTERFACE "cmsis-dap" CACHE STRING "Interface type to use with OpenOCD")

# Check if we can find an openocd binary

find_program(openocd_executable openocd)
find_package(Python 3 COMPONENTS Interpreter)

# Add libraries for the vendored libs

# hal
if (MSIGN_BUILD_TYPE STREQUAL "minisign")
	file(GLOB cube_ll_srcs ${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF4/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_*.c)
else()
	file(GLOB cube_ll_srcs ${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF2/Drivers/STM32F2xx_HAL_Driver/Src/stm32f2xx_ll_*.c)
endif()

add_library(cube_ll OBJECT ${cube_ll_srcs})

if (MSIGN_BUILD_TYPE STREQUAL "minisign")
	target_include_directories(cube_ll PUBLIC
		${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF4/Drivers/STM32F4xx_HAL_Driver/Inc
		${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF4/Drivers/CMSIS/Device/ST/STM32F4xx/Include
		${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF4/Drivers/CMSIS/Include
	)
else()
	target_include_directories(cube_ll PUBLIC
		${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF2/Drivers/STM32F2xx_HAL_Driver/Inc
		${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF2/Drivers/CMSIS/Device/ST/STM32F2xx/Include
		${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF2/Drivers/CMSIS/Include
	)
endif()

target_include_directories(cube_ll PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/cfg
)

target_compile_definitions(cube_ll PUBLIC
	USE_FULL_LL_DRIVER
)

target_compile_options(cube_ll PUBLIC
	$<$<COMPILE_LANGUAGE:CXX>: -fno-threadsafe-statics -Wno-register -Wno-volatile>
)

if (MSIGN_BUILD_TYPE STREQUAL "board")
	target_compile_definitions(cube_ll PUBLIC
		-DSTM32F205xx
	)
elseif (MSIGN_BUILD_TYPE STREQUAL "nucleo")
	target_compile_definitions(cube_ll PUBLIC
		-DSTM32F207xx
	)
elseif (MSIGN_BUILD_TYPE STREQUAL "minisign")
	target_compile_definitions(cube_ll PUBLIC
		-DSTM32F405xx
	)
endif()


set(freertos_dir ${CMAKE_CURRENT_LIST_DIR}/../vendor/FreeRTOS)

# rtos
# compiled against cm3 because we don't use the cm4 fpu
add_library(freertos OBJECT
	${freertos_dir}/event_groups.c
	${freertos_dir}/list.c
	${freertos_dir}/queue.c
	${freertos_dir}/stream_buffer.c
	${freertos_dir}/tasks.c

	${freertos_dir}/portable/GCC/ARM_CM3/port.c
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

file(GLOB_RECURSE stm_crash_srcs 
	${CMAKE_CURRENT_SOURCE_DIR}/src/crash/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/crash/*.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/crash/*.s
)

list(REMOVE_ITEM stm_srcs ${stm_crash_srcs})

add_library(stm_crash STATIC ${stm_crash_srcs})

add_executable(stm ${stm_srcs})

target_link_libraries(stm_crash
	cube_ll
	freertos
)

if (MSIGN_BUILD_TYPE STREQUAL "minisign")
	target_compile_definitions(stm_crash PUBLIC USE_F4)
else()
	target_compile_definitions(stm_crash PUBLIC USE_F2)
endif()

set_property(TARGET stm_crash PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
set_property(TARGET freertos PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/nvic.cpp PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)

target_link_libraries(stm
	cube_ll
	freertos
	stm_crash

	c m gcc
)

target_compile_options(stm
	PUBLIC
	-O3
	-ffast-math
)

target_include_directories(stm_crash
	PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/../vendor/TinyDeflate
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

execute_process(COMMAND ${CMAKE_LINKER} --help OUTPUT_VARIABLE MSIGN_LINKER_HELP)
if ("${MSIGN_LINKER_HELP}" MATCHES ".*warn-rwx.*")
	set(USE_NO_WARN_RWX 1)
endif()

target_link_options(stm
	PUBLIC
	-Wl,-T${CMAKE_CURRENT_SOURCE_DIR}/ld/$<IF:$<STREQUAL:${MSIGN_BUILD_TYPE},minisign>,f4.ld,f2.ld>
	-Wl,--undefined=uxTopUsedPriority
	-Wl,--gc-sections,--relax
	-Wl,--nmagic
	-Wl,-Map=stm.map
	$<$<BOOL:${USE_NO_WARN_RWX}>:-Wl,--no-warn-rwx-segment>
	--specs=nano.specs
)

# add debug generation code
add_custom_command(TARGET stm
	POST_BUILD
	COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/dbg/create_blob.py $<TARGET_FILE:stm> ${CMAKE_CURRENT_BINARY_DIR}/stm.dbg.bin
	COMMAND ${OBJCOPY_EXECUTABLE} --set-section-flags .fw_dbg=ALLOC,LOAD,CONTENTS,READONLY,DATA --update-section .fw_dbg=${CMAKE_CURRENT_BINARY_DIR}/stm.dbg.bin $<TARGET_FILE:stm> > ${CMAKE_CURRENT_BINARY_DIR}/stm.dbg.log 2>&1
	BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/stm.dbg.bin ${CMAKE_CURRENT_BINARY_DIR}/stm.dbg.log
	COMMENT Generating and appending debug blob
)

if (EXISTS ${openocd_executable})
	message(STATUS "Found OPENOCD " ${openocd_executable})

	if (MSIGN_BUILD_TYPE STREQUAL "minisign")
		set(upload_cfg stm32f4x.cfg)
	else()
		set(upload_cfg stm32f2x.cfg)
	endif()

	# add upload target
	add_custom_target(upload
		COMMAND ${openocd_executable} -f interface/${MSIGN_OCD_INTERFACE}.cfg -f target/${upload_cfg} -c "program $<TARGET_FILE:stm> verify reset exit" || echo "program $<TARGET_FILE:stm> verify reset" | telnet localhost 4444
	)

	add_dependencies(upload stm)
else()
	message(STATUS "Disabling upload target due to no OpenOCD")
endif()

add_custom_target(stm.bin ALL DEPENDS stm COMMAND ${OBJCOPY_EXECUTABLE} -Obinary $<TARGET_FILE:stm> ${CMAKE_CURRENT_BINARY_DIR}/stm.bin)
