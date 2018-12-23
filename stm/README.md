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

Uses the concept of "slots"; the stm can request up to 256 simultaneous data streams of up to 16 bytes each, either polling for new data or always updating it as new data is available.

The protocol starts with a handshake, initiated from the STM32, and continues asynchronously.

### Command format

```
| 0xA6 | 0x00 | 0x11 | <payload> |
  |       |     |
  |       |     ------ command, see below
  |       ---- payload size
  --- direction byte, A6 for packets from the esp, A5 for packets from the STM
```

| Name | Byte |
| ---- | ---- |
| `HANDSHAKE_INIT` | `0x10` |
| `HANDSHAKE_RESP` | `0x11` |
| `HANDSHAKE_OK` | `0x12` |
| `OPEN_CONN` | `0x20` |
| `CLOSE_CONN` | `0x21` |
| `NEW_DATA` | `0x22` |
| `ACK_OPEN_CONN` | `0x30` |
| `ACK_CLOSE_CONN` | `0x31` |
| `SLOT_DATA` | `0x40` |

### Handshake

STM sends command `HANDSHAKE_INIT`, esp responds with `HANDSHAKE_RESP`, stm finishes with `HANDSHAKE_OK`. All commands have payload size 0 and no payload.

### Slots

A slot can be in two modes, continuous or polled. Slots have an associated data ID, which tells the ESP what data to send the STM.

#### Managing slots

To open a slot, one sends the `OPEN_CONN` command. This command's payload is as follows.

```
| 0x00 | 0xFF | 0x01 | 0x00 |
  |      |      |      |
  |      |      |      |
  |      |      ------------ 16 bit little endian data id; 0 is reserved
  |      ---- slot id
  ------ continuous enable; 1 for continuous, 0 for polled
```

Note that a polled slot automatically gets new data as soon as it is opened.

Closing a slot is done with the `CLOSE_CONN` command, whose payload is a single byte containing the slot ID to close.

Both of these commands get replies from the ESP, called `ACK_OPEN_CONN` and `ACK_CLOSE_CONN` respectively.
These two commands both have the same payload data, containing the slot ID that was opened or closed.

##### Getting data

Continuous slots will periodically send the `SLOT_DATA` command from the ESP.
Polled slots must send the `NEW_DATA` command from the STM, containing the slot ID to update as its payload.

The `SLOT_DATA` command's format is as follows.

```
| 0xFF | <slot_data>
  |      |
  |      ---- slot data. size is between 0-16, given by payload length - 1.
  --- slot ID
```

It is the only variable-length packet currently in the protocol. Data past the end of the payload but before the 16-byte cutoff should be interpreted as zero.

## Serial

| Pin  | Mapped |
| ---- | ------ |
| `TX` | `PG14`  |
| `RX` | `PG9`  |
