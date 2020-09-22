file(GLOB_RECURSE msign_esp_srcs FOLLOW_SYMLINKS
	${CMAKE_CURRENT_LIST_DIR}/*.cpp
	${CMAKE_CURRENT_LIST_DIR}/*.c
	)

file(GLOB msign_nmfus ${CMAKE_CURRENT_LIST_DIR}/parser/*.nmfu)

idf_component_register(SRCS ${msign_esp_srcs})

if (NOT CMAKE_BUILD_EARLY_EXPANSION)
	foreach(nmfu_file ${msign_nmfus})
		get_filename_component(nmfu_target_name ${nmfu_file} NAME_WE)
		list(APPEND msign_esp_srcs ${CMAKE_CURRENT_BINARY_DIR}/parser/${nmfu_target_name}.c)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/parser/${nmfu_target_name}.c ${CMAKE_CURRENT_BINARY_DIR}/parser/${nmfu_target_name}.h
			COMMAND ${NMFU_EXECUTABLE} -O3 ${nmfu_file}
			DEPENDS ${nmfu_file}
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/parser
		)
		list(APPEND msign_nmfus_gen ${CMAKE_CURRENT_BINARY_DIR}/parser/${nmfu_target_name}.c ${CMAKE_CURRENT_BINARY_DIR}/parser/${nmfu_target_name}.h)
	endforeach(nmfu_file)

	add_custom_target(msign_parsers DEPENDS ${msign_nmfus_gen})

	find_program(NMFU_EXECUTABLE nmfu REQUIRED)
	message(STATUS "Found NMFU: " ${NMFU_EXECUTABLE})

	set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 14)
	target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}parser)
	add_dependencies(${COMPONENT_LIB} msign_parsers)

	message(STATUS "Incdir " ${CMAKE_CURRENT_BINARY_DIR}/parser " from " ${msign_nmfus_gen})
endif()
