set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

set(TOOLCHAIN_TRIPLE arm-none-eabi)

find_program(GCC_COMPILER arm-none-eabi-gcc)
get_filename_component(ARM_TOOLCHAIN_DIR ${GCC_COMPILER} DIRECTORY)

set(ARM_GCC_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
execute_process(COMMAND ${GCC_COMPILER} -print-sysroot
    OUTPUT_VARIABLE ARM_GCC_SYSROOT OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND ${GCC_COMPILER} -print-multi-directory -mcpu=cortex-m3 -mfloat-abi=soft
	OUTPUT_VARIABLE ARM_GCC_MULTIDIR OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND ${GCC_COMPILER} -dumpversion
	OUTPUT_VARIABLE ARM_GCC_VERNUM OUTPUT_STRIP_TRAILING_WHITESPACE)

set(triple arm-none-eabi)
set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_COMPILER_TARGET ${triple})

set(CMAKE_C_FLAGS_INIT "-B${ARM_GCC_SYSROOT} -fshort-enums -nostdlib -mthumb -mcpu=cortex-m3 -mfloat-abi=soft -ggdb3")
set(CMAKE_CXX_FLAGS_INIT "-B${ARM_GCC_SYSROOT} -fshort-enums -stdlib=libstdc++ -nostdlib -mthumb -mcpu=cortex-m3 -mfloat-abi=soft -ggdb3 -fno-exceptions -fno-rtti -Wno-register -isystem ${ARM_GCC_SYSROOT}/include/c++/${ARM_GCC_VERNUM}/arm-none-eabi/${ARM_GCC_MULTIDIR}/")
# Without that flag CMake is not able to pass test compilation check
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(OBJCOPY_EXECUTABLE ${ARM_TOOLCHAIN_DIR}/arm-none-eabi-objcopy)
set(CMAKE_C_LINKER ${ARM_TOOLCHAIN_DIR}/arm-none-eabi-gcc)
set(CMAKE_CXX_LINKER ${ARM_TOOLCHAIN_DIR}/arm-none-eabi-g++)
set(CMAKE_AR ${ARM_TOOLCHAIN_DIR}/arm-none-eabi-ar)
set(CMAKE_ASM_COMPILER ${ARM_TOOLCHAIN_DIR}/arm-none-eabi-gcc)

set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINKER} -fshort-enums -nostdlib -mthumb -mcpu=cortex-m3 -mfloat-abi=soft -ggdb3 -fno-exceptions -fno-rtti -Wno-register <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINKER} -fshort-enums -nostdlib -mthumb -mcpu=cortex-m3 -mfloat-abi=soft -ggdb3 <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

set(CMAKE_SYSROOT ${ARM_GCC_SYSROOT})
set(CMAKE_FIND_ROOT_PATH ${ARM_GCC_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
