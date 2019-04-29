# stmboot

This is the STM bootloader.

It is responsible for updating the stm with new code, as well as booting said new code.

## Flash Layout

```
0x0800 0000        										
|              						 0x080F C000
|     0x0800 4000    0x0808 0000     |  0x080F C000 + sizeof(bootcmd_t)
|     |              |               |  | end of flash
|     |  496 KiB     |  496 KiB      |  | |
+-----+--------------+---------------+--+-+
|     |  APPLICATION |  UPDATE PKG   |  | |
+-----+--------------+---------------+--+-+
|                                    |  |              
|                                    |  --- update detector, 0xFF after an update, can be cleared with flash programming functions.
-- bootloader                        -- update params
```

## BCMD

The `BCMD` struct, also known as the "update params" can be used to tell the bootloader to do something. By default, the bootloader will simply run the code (it might reset first if the structure is damaged).

By writing a `BOOTCMD_UPDATE` or 1 into the `cmd` byte, the bootloader will attempt to update the code currently residing in the `APPLICATION` partition with that of the `UPDATE PKG` one. The `size` value
_must_ be set to the size of the new code, otherwise the bootloader might just erase the entire partition.

The signature bytes should be left alone, but can be checked to be equal to `0xae 0x7d` to see if the BCMD struct actually exists.

When the update is finished, all of sector 11 is erased, which means that some of the update itself, but more importantly the area at the end of flash will be erased. 
This can be used to determine if the code is coming out of an update -- and the flash can be written here to a zero once this is detected. 
