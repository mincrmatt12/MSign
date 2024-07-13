# Builds the STM bootloader binaries
cmake_minimum_required(VERSION 3.13)
project(msign-stmboot C ASM)

set(CMAKE_CXX_STANDARD 17)

# specify target
set(MSIGN_BUILD_TYPE "board" CACHE STRING "Build target, either 'nucleo' or 'board' or 'minisign'")
set(MSIGN_OCD_INTERFACE "cmsis-dap" CACHE STRING "Interface type to use with OpenOCD")

# find library
set(cube_ll_dir ${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF2/Drivers/STM32F2xx_HAL_Driver/Inc)

if (MSIGN_BUILD_TYPE STREQUAL "minisign")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m4")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-m4")
else()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m3")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-m3")
endif()

# Check if we can find an openocd binary
find_program(openocd_executable openocd)

add_executable(stmboot
	src/main.c
	src/startup.s
	src/memset.c
	
	src/common/bootcmd.c
)

if (MSIGN_BUILD_TYPE STREQUAL "minisign")
	target_include_directories(stmboot PRIVATE
		${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF4/Drivers/STM32F4xx_HAL_Driver/Inc
		${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF4/Drivers/CMSIS/Device/ST/STM32F4xx/Include
		${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF4/Drivers/CMSIS/Include
	)
else()
	target_include_directories(stmboot PRIVATE
		${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF2/Drivers/STM32F2xx_HAL_Driver/Inc
		${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF2/Drivers/CMSIS/Device/ST/STM32F2xx/Include
		${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF2/Drivers/CMSIS/Include
	)
endif()

if (MSIGN_BUILD_TYPE STREQUAL "board")
	target_compile_definitions(stmboot PRIVATE
		-DSTM32F205xx
		-DUSE_F2
	)
elseif (MSIGN_BUILD_TYPE STREQUAL "nucleo")
	target_compile_definitions(stmboot PRIVATE
		-DSTM32F207xx
		-DUSE_F2
	)
elseif (MSIGN_BUILD_TYPE STREQUAL "minisign")
	target_compile_definitions(stmboot PRIVATE
		-DSTM32F405xx
		-DUSE_F4
	)
endif()

target_compile_options(stmboot PRIVATE
	$<IF:$<C_COMPILER_ID:Clang>,-Oz,-Os>
	-ffreestanding
)

target_link_options(stmboot PRIVATE
	-Wl,-T${CMAKE_CURRENT_SOURCE_DIR}/ld/boot.ld
	-Wl,--nmagic
	-fno-builtin
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
		COMMAND ${openocd_executable} -f interface/${MSIGN_OCD_INTERFACE}.cfg -f target/${upload_cfg} -c "program $<TARGET_FILE:stmboot> verify reset exit"
	)

	add_dependencies(upload stmboot)
else()
	message(STATUS "Disabling upload target due to no OpenOCD")
endif()

add_custom_target(stmboot.bin ALL DEPENDS stmboot COMMAND ${OBJCOPY_EXECUTABLE} -Obinary $<TARGET_FILE:stmboot> ${CMAKE_CURRENT_BINARY_DIR}/stmboot.bin)
