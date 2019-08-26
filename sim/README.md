# sim

Simulates the entire sign, using mostly the same codebase.

## Building

Requirements:
	- a recent clone of the Arduino core for the esp8266, with my patches installed.
	- a very recent version of CMake
	- the rest of the source tree

Point `ARDUINO_ESP_PATH` at that clone, then do a normal CMake setup. Building should be entirely automatic at this point.

## Running

To simply run the sign, use named pipes.

Create two with

```
$ mkfifo stoe
$ mkfifo etos
```

then point the new `stmsim` and `espsim` binaries at them:

```
$ # terminal 1
$ ./stmsim <etos 2>stoe

----

$ # terminal 2
$ ./espsim <stoe >etos
```

You can also use tee if you want to record the messages, possibly to be looked at with `caplog`.

## Structure

The `src` folder contains files to be mixed with the actual sources, any files here have precedence over those elsewhere, allowing for "overriding" of the original sources with mocked-up versions.
The two binaries communicate over standard file streams.

`espsim` sends data to `stmsim` over `stdout` and receives over `stdin`, sending debug data to `stderr`. To allow for terminal coloring, `stmsim` sends over `stderr`, but still receives on `stdin`.

The only file the fake ESP currently reads is the `config.txt`, placed in the working directory and with exactly identical contents to what it would be on the real sign.
