# TOOLCHAIN SETUP
find_program(C_COMPILER arm-none-eabi-gcc)
get_filename_component(TOOLCHAIN_DIR ${C_COMPILER} DIRECTORY)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_DIR}/arm-none-eabi-gcc)
set(OBJCOPY_EXECUTABLE ${TOOLCHAIN_DIR}/arm-none-eabi-objcopy)

# make cmake build check happy
set(CMAKE_EXE_LINKER_FLAGS "--specs=nosys.specs")
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")

set(CMAKE_C_FLAGS "-nostdlib -mthumb -mcpu=cortex-m3")
set(CMAKE_CXX_FLAGS "-nostdlib -mthumb -mcpu=cortex-m3 -fno-exceptions -fno-rtti -Wno-register")
