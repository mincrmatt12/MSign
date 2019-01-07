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
|     |  APPLICATION |  UPDATE PKG   |  |X|
+-----+--------------+---------------+--+-+
|                                    |                
|                                    --- update params
-- bootloader       
```
