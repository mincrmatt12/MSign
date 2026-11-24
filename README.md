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

- A recent `arm-none-eabi-gcc` installation (suitably modern; currently tested against GCC 15, you need at least C++20)
- A recent `xtensa-esp8266-elf-gcc` (the 'experimental' compiler provided by recent Espressif releases, again you need C++20 support and functioning newlib; we use GCC 16)
- Python 3 and the packages in `requirements.txt`
  - If you aren't building the `espweb` directory you do not require the `cryptography` package listed
- CMake (version 3.13 or later is recommended - probably not too much later since the ESP8266 RTOS SDK is old and CMake will probably eventually drop support for its archaisms)
- OpenOCD (for flashing)
- `quilt`, to apply the patches in `vendor` for the ESP8266 SDK

and everything checked out in the `vendor` directory. The ESP8266 SDK also usually requests you have python (specifically _system_ python, whatever runs from `python`) installed with it's requirements (`vendor/ESP8266_RTOS_SDK/requirements.txt`)
Once you've checked out the submodules, apply the quilt patch stack to get the ESP8266's SDK in a state where it will cooperate.

A Docker file with these is provided as `Dockerfile.build`.

For building the `espweb` you also need:

- yarn (v4)
- node (25 or later)

A Docker file with _these_ is also provided as `Dockerfile.webbuild`; which also has a python install with the correct dependencies to be able to build the CA cert archives (basically everything
necessary on the SD card)

In general, the Jenkins is setup to autobuild everything, so whatever it does should work.
