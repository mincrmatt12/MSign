# STM codebase

LEDSignCodebase, not public because it'll probably get api keys at some point.

## HUB75

| Pin   | Mapped |
| ----- | ------ |
| `R0`  | `PD0`  |
| `G0`  | `PD1`  |
| `B0`  | `PD2`  |
| `R1`  | `PD3`  |
| `G1`  | `PD4`  |
| `B1`  | `PD5`  |
| `A`   | `PB0`  |
| `B`   | `PB1`  |
| `C`   | `PB2`  |
| `D`   | `PB3`  |
| `CLK` | `PD6`  |
| `LAT` | `PB5`  |
| `OE`  | `PB6`  |

Uses DMA to the GPIO to drive R0-B1 as well as the clock.
DMA is triggered off of TIM1 update event; as is the delay between different bits of color in PCM encoding.

## Protocol

See the code / esp8266 readme.

## Serial

| Pin  | Mapped |
| ---- | ------ |
| `TX` | `PG14`  |
| `RX` | `PG9`  |
