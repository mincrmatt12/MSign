cmake_minimum_required(VERSION 3.5)

set(COMPONENTS pthread esp8266 main freertos lwip esp_common bootloader partition_table spi_flash esptool_py log bearssl_supplicant bearssl fatfs_priv libb64 app_update crashlogs)
set(PYTHON_DEPS_CHECKED 1) # no longer works in modern python

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(msign-esp)

idf_build_set_property(COMPILE_OPTIONS "-Wno-error=format-truncation" APPEND)
