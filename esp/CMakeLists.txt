cmake_minimum_required(VERSION 3.5)

set(COMPONENTS pthread esp8266 main freertos lwip esp_common bootloader partition_table spi_flash esptool_py log bearssl fatfs libb64 app_update)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(msign-esp)

