cmake_minimum_required(VERSION 3.15)
project(signsim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
set(SIM_FREERTOS_DIR ${CMAKE_CURRENT_LIST_DIR}/../vendor/FreeRTOS)
set(SIM_LWIP_DIR ${CMAKE_CURRENT_LIST_DIR}/../vendor/lwip/src)
set(SIM_BRSSL_PATH ${CMAKE_CURRENT_LIST_DIR}/../vendor/BearSSL)
set(SIM_FATFS_PATH ${CMAKE_CURRENT_LIST_DIR}/../esp/components/fatfs)
set(SIM_B64_PATH ${CMAKE_CURRENT_LIST_DIR}/../esp/components/libb64)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_program(NMFU_EXECUTABLE nmfu REQUIRED)
message(STATUS "Found NMFU: " ${NMFU_EXECUTABLE})

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   add_compile_options (-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
   add_compile_options (-fcolor-diagnostics)
endif ()

if (${USE_ASAN})
	add_compile_options(-fno-omit-frame-pointer -fsanitize=address)
	add_link_options(-fno-omit-frame-pointer -fsanitize=address)
endif()

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
file(GLOB_RECURSE STM_OVERRIDE_SRCS ${CMAKE_CURRENT_LIST_DIR}/src/stm/*.[ch]*)
file(GLOB_RECURSE STM_SRCS FOLLOW_SYMLINKS ${CMAKE_CURRENT_LIST_DIR}/../stm/src/*.[ch]*)

file(GLOB_RECURSE ESP_OVERRIDE_SRCS ${CMAKE_CURRENT_LIST_DIR}/src/esp/*.[ch]*)
file(GLOB_RECURSE ESP_SRCS FOLLOW_SYMLINKS ${CMAKE_CURRENT_LIST_DIR}/../esp/main/*.[ch]*)
file(GLOB         ESP_NMFUS ${CMAKE_CURRENT_LIST_DIR}/../esp/main/parser/*.nmfu)

# now, create a new list, called TOTAL_STM_SRCS, from the original list
set(TOTAL_STM_SRCS ${STM_SRCS})
list(TRANSFORM TOTAL_STM_SRCS REPLACE "(.*)/.../src" "stmsrc")
list(TRANSFORM STM_OVERRIDE_SRCS REPLACE "(.*)/.../src/stm" "stmsrc")

# concatenate the lists
list(APPEND TOTAL_STM_SRCS ${STM_OVERRIDE_SRCS})
list(REMOVE_DUPLICATES TOTAL_STM_SRCS)
list(TRANSFORM TOTAL_STM_SRCS PREPEND ${CMAKE_CURRENT_BINARY_DIR}/)

# do the same for the ESP
set(TOTAL_ESP_SRCS ${ESP_SRCS})
list(TRANSFORM TOTAL_ESP_SRCS REPLACE "(.*)/.../main" "espsrc")
list(TRANSFORM ESP_OVERRIDE_SRCS REPLACE "(.*)/.../src/esp" "espsrc")

# concatenate the lists
list(APPEND TOTAL_ESP_SRCS ${ESP_OVERRIDE_SRCS})
list(REMOVE_DUPLICATES TOTAL_ESP_SRCS)
list(TRANSFORM TOTAL_ESP_SRCS PREPEND ${CMAKE_CURRENT_BINARY_DIR}/)

message(STATUS "Creating copy targets")

# c) create copy targets
add_custom_target(copy_stm_src)
add_custom_command(TARGET copy_stm_src PRE_BUILD
	COMMENT "Copying stm sources" 
	BYPRODUCTS ${TOTAL_STM_SRCS}
	COMMAND python3 ${CMAKE_CURRENT_LIST_DIR}/cmake/copy.py
	${CMAKE_CURRENT_LIST_DIR}/../stm/src ${CMAKE_CURRENT_BINARY_DIR}/stmsrc ${CMAKE_CURRENT_LIST_DIR}/src/stm
)

add_custom_target(copy_esp_src)
add_custom_command(TARGET copy_esp_src PRE_BUILD
	COMMENT "Copying esp sources" 
	BYPRODUCTS ${TOTAL_ESP_SRCS}
	COMMAND python3 ${CMAKE_CURRENT_LIST_DIR}/cmake/copy.py
	${CMAKE_CURRENT_LIST_DIR}/../esp/main ${CMAKE_CURRENT_BINARY_DIR}/espsrc ${CMAKE_CURRENT_LIST_DIR}/src/esp
)

# mark the files as generated
foreach(I IN LISTS TOTAL_STM_SRCS)
	set_source_files_properties(${I} PROPERTIES GENERATED TRUE)
endforeach()

foreach(I IN LISTS TOTAL_ESP_SRCS)
	set_source_files_properties(${I} PROPERTIES GENERATED TRUE)
endforeach()

# e) setup nmfu
foreach(nmfu_file ${ESP_NMFUS})
	get_filename_component(nmfu_target_name ${nmfu_file} NAME_WE)
	list(APPEND TOTAL_ESP_SRCS ${CMAKE_CURRENT_BINARY_DIR}/${nmfu_target_name}.c)
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${nmfu_target_name}.c ${CMAKE_CURRENT_BINARY_DIR}/${nmfu_target_name}.h
		COMMAND ${NMFU_EXECUTABLE} -O3 ${nmfu_file} -finclude-user-ptr $<$<STREQUAL:${nmfu_target_name},http_serve>:-findirect-start-ptr>
		DEPENDS ${nmfu_file}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
	list(APPEND ESP_NMFU_SRCS ${CMAKE_CURRENT_BINARY_DIR}/${nmfu_target_name}.c ${CMAKE_CURRENT_BINARY_DIR}/${nmfu_target_name}.h)
endforeach(nmfu_file)

# ----- section two: compile stm -----

# setup freertos

add_library(stm_freertos STATIC
	${SIM_FREERTOS_DIR}/event_groups.c
	${SIM_FREERTOS_DIR}/list.c
	${SIM_FREERTOS_DIR}/queue.c
	${SIM_FREERTOS_DIR}/stream_buffer.c
	${SIM_FREERTOS_DIR}/tasks.c

	${SIM_FREERTOS_DIR}/portable/ThirdParty/GCC/Posix/port.c
	${SIM_FREERTOS_DIR}/portable/ThirdParty/GCC/Posix/utils/wait_for_event.c
)

target_include_directories(stm_freertos PUBLIC
	${SIM_FREERTOS_DIR}/include
	${SIM_FREERTOS_DIR}/portable/ThirdParty/GCC/Posix

	${CMAKE_CURRENT_SOURCE_DIR}/cfg/stm
)

target_link_libraries(stm_freertos Threads::Threads)

add_executable(stmsim ${TOTAL_STM_SRCS})
file(RELATIVE_PATH BINARY_PATH_RELATIVE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_BINARY_DIR}/.)
target_include_directories(stmsim PRIVATE ${BINARY_PATH_RELATIVE}/stmsrc)
target_compile_options(stmsim PRIVATE -DSIM )
add_dependencies(stmsim copy_stm_src)

target_link_libraries(stmsim stm_freertos)

# ----- section three: setup ESP build -----
# A lot of this is stolen from https://github.com/esp8266/Arduino/blob/master/tests/host/Makefile

# a) add freertos

add_library(esp_freertos STATIC
	${SIM_FREERTOS_DIR}/event_groups.c
	${SIM_FREERTOS_DIR}/list.c
	${SIM_FREERTOS_DIR}/queue.c
	${SIM_FREERTOS_DIR}/stream_buffer.c
	${SIM_FREERTOS_DIR}/tasks.c

	${SIM_FREERTOS_DIR}/portable/ThirdParty/GCC/Posix/port.c
	${SIM_FREERTOS_DIR}/portable/ThirdParty/GCC/Posix/utils/wait_for_event.c
	${SIM_FREERTOS_DIR}/portable/MemMang/heap_3.c 
)

target_include_directories(esp_freertos PUBLIC
	${SIM_FREERTOS_DIR}/include
	${SIM_FREERTOS_DIR}/portable/ThirdParty/GCC/Posix

	${CMAKE_CURRENT_SOURCE_DIR}/cfg/esp
)

target_link_libraries(esp_freertos Threads::Threads)

# b) add lwip

add_library(esp_lwip STATIC
# COREFILES
	${SIM_LWIP_DIR}/core/init.c
	${SIM_LWIP_DIR}/core/def.c
	${SIM_LWIP_DIR}/core/dns.c
	${SIM_LWIP_DIR}/core/inet_chksum.c
	${SIM_LWIP_DIR}/core/ip.c
	${SIM_LWIP_DIR}/core/mem.c
	${SIM_LWIP_DIR}/core/memp.c
	${SIM_LWIP_DIR}/core/netif.c
	${SIM_LWIP_DIR}/core/pbuf.c
	${SIM_LWIP_DIR}/core/raw.c
	${SIM_LWIP_DIR}/core/stats.c
	${SIM_LWIP_DIR}/core/sys.c
	${SIM_LWIP_DIR}/core/tcp.c
	${SIM_LWIP_DIR}/core/tcp_in.c
	${SIM_LWIP_DIR}/core/tcp_out.c
	${SIM_LWIP_DIR}/core/timeouts.c
	${SIM_LWIP_DIR}/core/udp.c
# CORE4FILES
	${SIM_LWIP_DIR}/core/ipv4/autoip.c
	${SIM_LWIP_DIR}/core/ipv4/dhcp.c
	${SIM_LWIP_DIR}/core/ipv4/etharp.c
	${SIM_LWIP_DIR}/core/ipv4/icmp.c
	${SIM_LWIP_DIR}/core/ipv4/igmp.c
	${SIM_LWIP_DIR}/core/ipv4/ip4_frag.c
	${SIM_LWIP_DIR}/core/ipv4/ip4.c
	${SIM_LWIP_DIR}/core/ipv4/ip4_addr.c

    ${SIM_LWIP_DIR}/core/ipv6/dhcp6.c
	${SIM_LWIP_DIR}/core/ipv6/ethip6.c
	${SIM_LWIP_DIR}/core/ipv6/icmp6.c
	${SIM_LWIP_DIR}/core/ipv6/inet6.c
	${SIM_LWIP_DIR}/core/ipv6/ip6.c
	${SIM_LWIP_DIR}/core/ipv6/ip6_addr.c
	${SIM_LWIP_DIR}/core/ipv6/ip6_frag.c
	${SIM_LWIP_DIR}/core/ipv6/mld6.c
	${SIM_LWIP_DIR}/core/ipv6/nd6.c
#APIFILES
	${SIM_LWIP_DIR}/api/api_lib.c
	${SIM_LWIP_DIR}/api/api_msg.c
	${SIM_LWIP_DIR}/api/err.c
	${SIM_LWIP_DIR}/api/netbuf.c
	${SIM_LWIP_DIR}/api/netdb.c
	${SIM_LWIP_DIR}/api/netifapi.c
	${SIM_LWIP_DIR}/api/sockets.c
	${SIM_LWIP_DIR}/api/tcpip.c
#NETIF
	${SIM_LWIP_DIR}/netif/ethernet.c
#SNTP
	${SIM_LWIP_DIR}/apps/sntp/sntp.c
#ESPLWIP
	${CMAKE_CURRENT_SOURCE_DIR}/src/esplwip/sys_arch.c
	${CMAKE_CURRENT_SOURCE_DIR}/src/esplwip/netif/tapif.c
)
	
target_include_directories(esp_lwip PUBLIC
	${SIM_LWIP_DIR}/include
	#${SIM_LWIP_DIR}/include/posix
	${CMAKE_CURRENT_SOURCE_DIR}/src/esplwip/include

	${CMAKE_CURRENT_SOURCE_DIR}/cfg/esp
)

target_link_libraries(esp_lwip Threads::Threads esp_freertos)

# c) add bearssl

file(GLOB BEARSSL_SRCS
	${SIM_BRSSL_PATH}/src/aead/*.c
	${SIM_BRSSL_PATH}/src/codec/*.c
	${SIM_BRSSL_PATH}/src/ec/*.c
	${SIM_BRSSL_PATH}/src/hash/*.c
	${SIM_BRSSL_PATH}/src/int/*.c
	${SIM_BRSSL_PATH}/src/kdf/*.c
	${SIM_BRSSL_PATH}/src/mac/*.c
	${SIM_BRSSL_PATH}/src/rand/*.c
	${SIM_BRSSL_PATH}/src/rsa/*.c
	${SIM_BRSSL_PATH}/src/ssl/*.c
	${SIM_BRSSL_PATH}/src/symcipher/*.c
	${SIM_BRSSL_PATH}/src/x509/*.c
	${SIM_BRSSL_PATH}/src/*.c
)

add_library(esp_bearssl STATIC ${BEARSSL_SRCS})

target_include_directories(esp_bearssl PUBLIC
	${SIM_BRSSL_PATH}/inc
)

target_include_directories(esp_bearssl PRIVATE
	${SIM_BRSSL_PATH}/src
)

target_compile_definitions(esp_bearssl PUBLIC
	-DBR_USE_ALT_RAND -DBR_USE_UNIX_TIME
)

# d) add fatfs
add_library(esp_fatfs STATIC
	${SIM_FATFS_PATH}/ff.c
	${SIM_FATFS_PATH}/ffsystem.c
	${SIM_FATFS_PATH}/ffunicode.c
)

target_include_directories(esp_fatfs PUBLIC
	${SIM_FATFS_PATH}
)

target_link_libraries(esp_fatfs esp_freertos)

# e) add b64
add_library(esp_b64 STATIC
	${SIM_B64_PATH}/cencode.c
)

target_include_directories(esp_b64 PUBLIC
	${SIM_B64_PATH}/include
)

# e) add target for ESP build
add_custom_target(esp_parsers DEPENDS ${ESP_NMFU_SRCS})

add_executable(espsim ${TOTAL_ESP_SRCS})
target_compile_options(espsim PRIVATE -DSIM)
target_include_directories(espsim PRIVATE ${BINARY_PATH_RELATIVE}/espsrc ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(espsim copy_esp_src esp_parsers)

target_link_libraries(espsim esp_freertos esp_lwip esp_bearssl esp_fatfs esp_b64)
