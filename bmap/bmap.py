#!/usr/bin/env python3
import sys
import math
from PIL import Image

if len(sys.argv) != 2:
    print("Usage: bmap <image>")
    exit()
else:
    img = Image.open(sys.argv[1])

w, h = img.size

if len(img.getbands()) != 4:
    print("Need alpha channel")
    exit()

stride = int(math.ceil(w / 8))
data = []

for y in range(h):
    for x in range(w):
        if x % 8 == 0:
            data.append(0)
        if img.getpixel((x, y))[3] != 0:
            data[-1] |= (1 << (7 - (x % 8)))

print(f"// w={w}, h={h}, stride={stride}")
print("const uint8_t data = {")

for pos in range(0, len(data), stride):
    print("\t", end="")
    print(",".join(format(x, "#010b") for x in data[pos:pos+stride]), end="")
    if pos + stride < len(data):
        print(",")
    else:
        print("\n};")
