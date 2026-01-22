# MSign

Code + designs for an LED sign display board powered by an STM32 and ESP8266. Make sure you clone the submodules.

## Organization

- `board`: schematics and PCB layout
- `stm`: STM32 firmware (drives the display)
- `stmboot`: STM32 bootloader
- `esp`: ESP8266 firmware (grabs information from the internet)
- `espweb`: ESP8266 web-based configuration page source
- `cfgserver`: Server for shipping unattended config and software updates (without using the webui)
- `bmap`: Tools for generating graphics resources
- `caplog`: Tools for debugging
- `vendor`: Various dependencies in source form (everything not listed below)
- `sim`: Stubs/shims for running the `esp` and `stm` codebases under linux.

## Dependencies

- A recent `arm-none-eabi-gcc` installation (suitably modern to deal with C++20; GCC 14 is what's tested)
- A `xtensa-lx106-elf-gcc` that can deal with C++20; preferably version 13.2.0 (which was available at one point in debian)
  - A suitable newlib/libstdc++ archive is available in `Dockerfile.build`
- Python 3 and the packages in `requirements.txt`
  - If you aren't building the `espweb` directory you do not require the `cryptography` package listed
- OpenOCD (for flashing)
  - If you have PlatformIO installed we use its OpenOCD by default, so you may not need to install it system-wide.
- CMake (version 3.13 or later is recommended)
- `quilt`, to apply the patches in `vendor` for the ESP8266 SDK

and everything checked out in the `vendor` directory. The ESP8266 SDK also usually requests you have python (specifically _system_ python, whatever runs from `python`) installed with it's requirements (`vendor/ESP8266_RTOS_SDK/requirements.txt`)

A Docker file with these is provided as `Dockerfile.build`.

For building the `espweb` you also need:

- yarn
- node 

A Docker file with _these_ is also provided as `Dockerfile.webbuild`
