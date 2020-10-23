cmake_minimum_required(VERSION 3.5)

set(COMPONENTS esp8266 main freertos lwip esp_common bootloader partition_table spi_flash esptool_py log bearssl fatfs libb64 app_update)
set(IDF_PATH ${CMAKE_CURRENT_LIST_DIR}/../vendor/ESP8266_RTOS_SDK)
get_filename_component(IDF_PATH ${IDF_PATH} ABSOLUTE)
set(ENV{IDF_PATH} ${IDF_PATH})

include(${IDF_PATH}/tools/cmake/project.cmake)
project(msign-esp)

