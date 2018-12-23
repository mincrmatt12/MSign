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
