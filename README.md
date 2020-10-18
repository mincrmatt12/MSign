# SignCode

Code for an LED sign display board powered by an STM32 and ESP8266. Make sure you clone the submodules.

## Organization

- `board`: schematics and PCB layout
- `stm`: STM32 firmware (drives the display)
- `stmboot`: STM32 bootloader
- `esp`: ESP8266 firmware (grabs information from the internet)
- `espweb`: ESP8266 web-based configuration page source
- `bmap`: Tools for generating graphics resources
- `caplog`: Tools for debugging
- `vendor`: Various dependencies in source form.

## Dependencies

- A recent `arm-none-eabi-gcc` installation (recommended at least version 9, tested with version 10)
- A recent `xtensa-lx106-elf-gcc` installation (tested against 5.2)
- NMFU (version 0.2.0)
- CMake

and everything checked out in the `vendor` directory.

For building the `espweb` you need:

- yarn
- node 
