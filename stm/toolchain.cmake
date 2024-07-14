# TOOLCHAIN SETUP
find_program(C_COMPILER arm-none-eabi-gcc)
get_filename_component(TOOLCHAIN_DIR ${C_COMPILER} DIRECTORY)
get_filename_component(TOOLCHAIN_EXT ${C_COMPILER} LAST_EXT)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/arm-none-eabi-gcc${TOOLCHAIN_EXT})
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/arm-none-eabi-g++${TOOLCHAIN_EXT})
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_DIR}/arm-none-eabi-gcc${TOOLCHAIN_EXT})
set(CMAKE_LINKER ${TOOLCHAIN_DIR}/arm-none-eabi-ld${TOOLCHAIN_EXT})
set(CMAKE_AR ${TOOLCHAIN_DIR}/arm-none-eabi-gcc-ar${TOOLCHAIN_EXT})
set(CMAKE_RANLIB ${TOOLCHAIN_DIR}/arm-none-eabi-gcc-ranlib${TOOLCHAIN_EXT})
set(OBJCOPY_EXECUTABLE ${TOOLCHAIN_DIR}/arm-none-eabi-objcopy${TOOLCHAIN_EXT})

set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

# we always build with debug info because of the crash handler
set(CMAKE_C_FLAGS "-nostdlib -ffreestanding -fbuiltin -mthumb -mfloat-abi=soft -ggdb3")
set(CMAKE_CXX_FLAGS "-nostdlib -ffreestanding -fbuiltin -mthumb -mfloat-abi=soft -fno-exceptions -fno-rtti -Wno-register -ggdb3")
