# TOOLCHAIN SETUP
set(TOOLCHAIN_DIR /usr/local/arm-none-eabi)
if (NOT EXISTS ${TOOLCHAIN_DIR})
	message(WARNING "Trying to use PIO toolchain as backup; please install a new version of gccarmnoneeabi for better debug info into /usr/local")
	set(TOOLCHAIN_DIR $ENV{HOME}/.platformio/packages/toolchain-gccarmnoneeabi)
	if (NOT EXISTS ${TOOLCHAIN_DIR})
		message(FATAL_ERROR "Please build the bootloader first with platformio, as we steal it's dependencies")
	else()
		message(STATUS "Found toolchain at ${TOOLCHAIN_DIR}")
	endif()
else()
	set(TOOLCHAIN_DIR /usr/local)
endif()

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/bin/arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/bin/arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_DIR}/bin/arm-none-eabi-gcc)

# make cmake build check happy
set(CMAKE_EXE_LINKER_FLAGS "--specs=nosys.specs")
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")

set(CMAKE_C_FLAGS "-nostdlib -mthumb -mcpu=cortex-m3")
set(CMAKE_CXX_FLAGS "-nostdlib -mthumb -mcpu=cortex-m3 -fno-exceptions -fno-rtti -Wno-register")
