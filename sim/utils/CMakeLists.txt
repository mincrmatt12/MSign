add_library(esp_standalone_json STATIC ${ESP_PHYS_DIR}/json.cpp ${ESP_PHYS_DIR}/utf.cpp)
target_compile_definitions(esp_standalone_json PUBLIC -DSTANDALONE_JSON)
target_include_directories(esp_standalone_json PUBLIC ${ESP_PHYS_DIR})

add_executable(jdump ./jdump.cpp)
target_link_libraries(jdump esp_standalone_json)
