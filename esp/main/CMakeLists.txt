file(GLOB_RECURSE msign_esp_srcs FOLLOW_SYMLINKS
	${CMAKE_CURRENT_LIST_DIR}/*.cpp
	${CMAKE_CURRENT_LIST_DIR}/*.c
	)

file(GLOB msign_nmfus ${CMAKE_CURRENT_LIST_DIR}/parser/*.nmfu)

file(GLOB_RECURSE msign_esp_cfg_hdrs FOLLOW_SYMLINKS ${CMAKE_CURRENT_LIST_DIR}/*.cfg.h)

foreach(nmfu_file ${msign_nmfus})
	get_filename_component(nmfu_target_name ${nmfu_file} NAME_WE)
	list(APPEND msign_esp_srcs ${CMAKE_CURRENT_BINARY_DIR}/parser/${nmfu_target_name}.c)
endforeach(nmfu_file)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   add_compile_options (-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
   add_compile_options (-fcolor-diagnostics)
endif ()

cmake_policy(SET CMP0057 NEW)

if (NOT "${CMAKE_CURRENT_LIST_DIR}/config.cpp" IN_LIST msign_esp_srcs)
	list(APPEND msign_esp_srcs "config.cpp")
endif ()

idf_component_register(SRCS ${msign_esp_srcs})

macro(add_nmfu_target nmfu_target_name extra_flags)
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/parser/${nmfu_target_name}.c ${CMAKE_CURRENT_BINARY_DIR}/parser/${nmfu_target_name}.h
		COMMAND ${NMFU_EXECUTABLE} -O3 ${CMAKE_CURRENT_LIST_DIR}/parser/${nmfu_target_name}.nmfu ${extra_flags}
		DEPENDS ${CMAKE_CURRENT_LIST_DIR}/parser/${nmfu_target_name}.nmfu
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/parser
	)
endmacro()

find_program(NMFU_EXECUTABLE nmfu REQUIRED)
message(STATUS "Found NMFU: " ${NMFU_EXECUTABLE})
find_package(Python3 COMPONENTS Interpreter)

if (NOT CMAKE_BUILD_EARLY_EXPANSION)
	foreach(nmfu_file ${msign_nmfus})
		get_filename_component(nmfu_target_name ${nmfu_file} NAME_WE)
		list(APPEND msign_esp_srcs ${CMAKE_CURRENT_BINARY_DIR}/parser/${nmfu_target_name}.c)
		list(APPEND msign_nmfus_gen ${CMAKE_CURRENT_BINARY_DIR}/parser/${nmfu_target_name}.c ${CMAKE_CURRENT_BINARY_DIR}/parser/${nmfu_target_name}.h)
		set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/parser/${nmfu_target_name}.c PROPERTIES COMPILE_OPTIONS "-Wno-unused-label;-Wno-type-limits;-O3")
	endforeach(nmfu_file)

	add_custom_target(msign_parsers DEPENDS ${msign_nmfus_gen})

	add_nmfu_target(http_client "")
	add_nmfu_target(http_serve "-findirect-start-ptr")
	add_nmfu_target(multipart_header "")
	add_nmfu_target(tdsb_logintoken "")
	add_nmfu_target(ttc_rdf "-finclude-user-ptr")
	add_nmfu_target(gtfs_tripupdate "-finclude-user-ptr")
	add_nmfu_target(http_chunked_recv "-fyield-support" "-findirect-start-ptr")

	add_custom_command(OUTPUT ${CMAKE_CURRENT_LIST_DIR}/config.cpp
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/../confgen/generator.py ${msign_esp_cfg_hdrs} ${CMAKE_CURRENT_LIST_DIR}/config.cpp
		DEPENDS ${msign_esp_cfg_hdrs} ${CMAKE_CURRENT_LIST_DIR}/../confgen/generator.py
		COMMENT "Generating config parser..."
	)

	set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
	target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/parser)
	target_link_options(${COMPONENT_LIB} PUBLIC -Wl,-u -Wl,_printf_float)
endif()
