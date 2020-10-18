# fnter

A very simple header font file generator.
Takes in two parameters, the font file and it's size in pixels.

Spits out (to stdout) a header file which contains the data for the font.

All units in the file are in pixels; bitmaps are stored as shown below; kerning data is stored as a lexicographically sorted array.

```
abcdefgh ijk
00000000 10000000  | number of bits = width; number of bytes = stride

lmnopqrs tuv
00000001 01000000  | number of rows = height; size in bytes = stride * rows

abcdefghijk
lmnopqrstuv
```

# bmap

Essentially just the bitmap converter in fnter -- takes a png, searches for areas without transparency and maps them.

# testfnt

Small program to interact with the fonts from fnter
When configuring tell it the font to use with `-DTESTFNT_FONT_FILE=/some/font/file -DTESTFNT_FONT_SIZE=12`.

# obj2bin

Converts `obj` and `mtl` pairs to the binary format the sign uses for its internal 3d renderer.
