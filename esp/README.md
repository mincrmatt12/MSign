# ESP8266

## Pinouts

Debug tx is mapped to `GPIO2`.

TX/RX are on `GPIO13`/`GPIO15`.

SD is wired

| PIN | Mapped |
| --- | ------ |
| `MISO` | `GPIO12` |
| `MOSI` | `GPIO4` |
| `SCK` | `GPIO14` |
| `CS`  | `GPIO5` |

## Update protocol

Open on socket 34476.

Send the text `Mn` to handshake, nothing is returned.

### Commands

| CMD | Meaning |
| --- | ------ |
| `0x10` | Reset |
| `0x20` | Dump config, null terminated string returned |
| `0x30` | Update config |
| `0x40` | Update system |

#### Update config

Send the command, then the config file size, then finally the entire config file.

Returns one of three values

| Value | Meaning |
| --- | ------ |
| `0x10` | File too big |
| `0x11` | Failed to write to SD card |
| `0x00` | OK |

When done, the system resets.

#### Update system

This command has the following format

```

| 32 bit unsigned size of stm code | 

```
