# STM codebase

LEDSignCodebase, not public because it'll probably get api keys at some point.

The code will run on a nucleo-f207zg or the msign, which uses an stm32f205rgt

## HUB75

### Pinout on nucleo

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

### Pinout on board

| Pin   | Mapped |
| ----- | ------ |
| `R0`  | `PC0`  |
| `G0`  | `PC1`  |
| `B0`  | `PC2`  |
| `R1`  | `PC3`  |
| `G1`  | `PC4`  |
| `B1`  | `PC5`  |
| `A`   | `PB0`  |
| `B`   | `PB1`  |
| `C`   | `PB2`  |
| `D`   | `PB3`  |
| `CLK` | `PD6`  |
| `LAT` | `PB5`  |
| `OE`  | `PB6`  |

Uses DMA to the GPIO to drive R0-B1 as well as the clock.
DMA is triggered off of TIM1 update event; the delay between different bits of color in PCM encoding goes off of TIM9.

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
| `HANDSHAKE_UOK` | `0x13` |
| `OPEN_CONN` | `0x20` |
| `CLOSE_CONN` | `0x21` |
| `NEW_DATA` | `0x22` |
| `ACK_OPEN_CONN` | `0x30` |
| `ACK_CLOSE_CONN` | `0x31` |
| `SLOT_DATA` | `0x40` |
| `RESET` | `0x50` |
| `PING` | `0x51` |
| `PONG` | `0x52` |
| `UPDATE_CMD` | `0x60` |
| `UPDATE_IMG_DATA` | `0x61` |
| `UPDATE_IMG_START` | `0x62` |
| `UPDATE_STATUS` | `0x63` |
| `CONSOLE_MSG` | `0x70` |


### Handshake

STM sends command `HANDSHAKE_INIT`, esp responds with `HANDSHAKE_RESP`, STM finishes with `HANDSHAKE_OK`. All handshake commands have payload size 0 and no payload.
The command `HANDSHAKE_UOK` is sent to indicate that the STM should enter update mode.

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

#### Getting data

Continuous slots will periodically send the `SLOT_DATA` command from the ESP.
Polled slots MUST send the `NEW_DATA` command from the STM, containing the slot ID to update as its payload.

The `SLOT_DATA` command's format is as follows.

```
| 0xFF | <slot_data>
  |      |
  |      ---- slot data. size is between 0-16, given by payload length - 1.
  --- slot ID
```

It is the only variable-length packet currently in the protocol. Data past the end of the payload but before the 16-byte cutoff should be interpreted as zero.

### Consoles

A console is a unidirectional stream of characters. There are three consoles on the MSign:

| ID | Usage |
| --- | ---------|
| `0x01` | The debug console input; as accessed through the web interface for commands starting with `"stm"` |
| `0x02` | The debug console output; relayed back to the web interface, hooked to `stderr` |
| `0x10` | `printf` output; logs, hooked to `stdout` |

The protocol uses one message for these:

```
| 0x01 | <data> |
  |      |
  |		 - character data for the remainder of the message
  -------- the console ID
```

It can be sent from either device.

### Other commands

When either device sends the `RESET` command, both devices should reset. Usually sent by the ESP upon grabbing new configuration -- but the STM can also send it in rare cases.

When either device sends the `PING` command, the other MUST respond with the `PONG` command, UNLESS the handshake has not been completed.

The above three commands have no payload.

### Update commands

The update procedure is rather complicated due to the requirement to update two things.
The first step is the ESP receiving an update package. While it is downloading the package, it updates various status slots to inform. Once it has finished, it will send a `RESET` command to reset the device.
When the system reboots, the ESP shall respond with `HANDSHAKE_UOK` on successful handshaking. Once this occurs, the STM enter update mode. Once the update finishes for the STM, it must send an appropriate `UPDATE_STATUS` to the ESP. Once this occurs,
the ESP will update itself. When it finishes, the STM will hear a `UPDATE_CMD` to reset itself to normal mode. At this point, the system is updated.

#### Detailed flow

After the `RESET`, the ESP will send various `UPDATE_CMD`s, and then the image.

The `UPDATE_CMD` message looks like this:

```
| 0xFF |
 |
 ---- cmd ID
```

where `cmd ID` is:

| ID | Meaning |
| :--- | :--- |
| `0x10` | Cancel update mode / reset to normal mode |
| `0x11` | Prepare to recieve image
| `0x30` | Image read error |
| `0x31` | Image checksum error |
| `0x32` | Invalid state error |
| `0x40` | Update completed successfully |
| `0x50` | ESP wrote sector (see below) |
| `0x51` | ESP copying sector (see below) |

Once message `0x11` is sent, the STM should get ready to recieve the `UPDATE_IMG_START`. This message contains:

The STM can respond with `UPDATE_STATUS` messages, with the same format.

| ID | Meaning |
| :--- | ------ |
| `0x10` | Entered update mode |
| `0x12` | Ready for image |
| `0x13` | Ready for chunk |
| `0x20` | Beginning copy process |
| `0x21` | Copy process completed |
| `0x30` | Resend last chunk, csum error |
| `0x40` | Checksum error on entire thing, abort procedure |

Note there are no errors for copy process as if it fails the code won't run. If the ESP doesn't get anything for a few minutes it can report failure and ask for manual reflashing.

```
| 0xCC CC | 0xSS SS SS SS | 0xCNCN |
  |          |               |
  |          |               ------ chunck count, little endian 16bits
  |          ---- size of new image, bytes, little endian 32bits
  ---- checksum, little endian 16bits, CRC16
```

Then, the STM can send `UPDATE_STATUS` commands to get the chunks.

Once it sends the `0x13` message, the ESP responds with a chunk like this:

```
| 0xCC 0xCC | <data for rest of message>
  |            |
  |            -- chunk data (size = msgsize - 2) extra bytes are padded with zeros
  ---- CRC-16 checksum for this chunk
```

After each chunk, the STM can respond with an error or a new message.
If the stm deduces it has received all chunks, it sends the finished message and updates itself.
After completion of this process, it sends the status corresponding to update completed.
The ESP will then begin writing the image to itself in preparation for copying, and will send out "ESP wrote sector" messages
as it does so.
The STM then waits for the ESP to send a cmd corresponding to completed.
It then resets.

## Serial

### Pinout on nucleo

USART6.

| Pin  | Mapped |
| ---- | ------ |
| `TX` | `PG14`  |
| `RX` | `PG9`  |

### Pinout on board

USART2.

| Pin  | Mapped |
| ---- | ------ |
| `TX` | `PA2`  |
| `RX` | `PA3`  |
