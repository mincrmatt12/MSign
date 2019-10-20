# stmboot

This is the STM bootloader.

It is responsible for updating the stm with new code, as well as booting said new code.

## Flash Layout

```
0x0800 0000        										
|                                    
|     0x0800 4000    0x0808 0000     0x080F C000
|     |              |               |    end of flash
|     |  496 KiB     |  496 KiB      |    |
+-----+--------------+---------------+----+
|     |  APPLICATION |  UPDATE PKG   |    |
+-----+--------------+---------------+----+
|                                    |  
|                                    --- unused; might contain recovery firmware at some point
-- bootloader                        
```

## Protocol

Talks to the application code using the RTC backup registers; which are preserved across resets.
The layout is as follows:

| Register number | Description |
| --------------- | ----------- |
| `0x00` | Bootloader command, either 0 for run application or 1 for update. |
| `0x01` | Update status flag, 0 if booting normally, anything else indicates booting from an update |
| `0x02` | Update size, i.e. number of bytes to copy from `0x0808 0000` to `0x0800 4000` for an update |

The application should clear the update status flag, as if either

- the bootloader starts with the update status flag enabled
- the program returns control to the bootloader
- any of the interrupt handlers fire from the bootloader (i.e. the app didn't change `VTOR` in the `SCB`)

the bootloader will enter recovery mode (showing a pattern of dots on the matrix) and waiting for an update mode from the ESP.
(todo)
