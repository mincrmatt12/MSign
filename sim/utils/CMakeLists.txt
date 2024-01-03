add_library(esp_standalone_json STATIC ${ESP_PHYS_DIR}/json.cpp ${ESP_PHYS_DIR}/utf.cpp)
target_compile_definitions(esp_standalone_json PUBLIC -DSTANDALONE_JSON)
target_include_directories(esp_standalone_json PUBLIC ${ESP_PHYS_DIR})

add_library(esp_standalone_weathersummarizer STATIC ${ESP_PHYS_DIR}/grabber/weathersummarizer.cpp)
target_include_directories(esp_standalone_weathersummarizer PUBLIC ${ESP_PHYS_DIR}/grabber)
target_compile_definitions(esp_standalone_weathersummarizer PRIVATE -DWEATHERSUMMARIZER_TEST)
target_link_libraries(esp_standalone_weathersummarizer esp_standalone_json)

add_executable(jdump ./jdump.cpp)
target_link_libraries(jdump esp_standalone_json)

add_executable(wtest ./wtest.cpp)
target_link_libraries(wtest esp_standalone_weathersummarizer esp_standalone_json)
