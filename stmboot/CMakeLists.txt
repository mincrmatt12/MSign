# Builds the STM bootloader binaries
cmake_minimum_required(VERSION 3.13)
project(msign-stmboot C ASM)

set(CMAKE_CXX_STANDARD 17)

# specify target
set(MSIGN_BUILD_TYPE "board" CACHE STRING "Build target, either 'nucleo' or 'board' or 'board2'")

# find library
set(cube_ll_dir ${CMAKE_CURRENT_LIST_DIR}/../vendor/STM32CubeF2/Drivers/STM32F2xx_HAL_Driver/Inc)

# Check if we can find an openocd binary
find_program(openocd_executable
	openocd
	PATHS $ENV{HOME}/.platformio/packages/tool-openocd/bin ENV OPENOCD_DIR
	NO_DEFAULT_PATH
)

add_executable(stmboot
	src/main.c
	src/startup.s
	
	src/common/bootcmd.c
)

if (MSIGN_BUILD_TYPE STREQUAL "board2")
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
		-DF_CPU=120000000L
		-DUSE_F2
	)
elseif (MSIGN_BUILD_TYPE STREQUAL "nucleo")
	target_compile_definitions(stmboot PRIVATE
		-DSTM32F207xx
		-DF_CPU=120000000L
		-DUSE_F2
	)
elseif (MSIGN_BUILD_TYPE STREQUAL "board2")
	target_compile_definitions(stmboot PRIVATE
		-DSTM32F405xx
		-DF_CPU=120000000L
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
	--specs=nano.specs
)

target_link_libraries(stmboot
	c
)

if (EXISTS ${openocd_executable})
	message(STATUS "Found OPENOCD " ${openocd_executable})

	if (MSIGN_BUILD_TYPE STREQUAL "board2")
		set(upload_cfg stm32f2x.cfg)
	else()
		set(upload_cfg stm32f4x.cfg)
	endif()

	# add upload target
	add_custom_target(upload
		COMMAND ${openocd_executable} -f interface/stlink.cfg -f target/${upload_cfg} -c "program $<TARGET_FILE:stmboot> verify reset exit"
	)

	add_dependencies(upload stmboot)
else()
	message(STATUS "Disabling upload target due to no OpenOCD")
endif()

add_custom_target(stmboot.bin ALL DEPENDS stmboot COMMAND ${OBJCOPY_EXECUTABLE} -Obinary $<TARGET_FILE:stmboot> ${CMAKE_CURRENT_BINARY_DIR}/stmboot.bin)
