#!/usr/bin/env python3
import time
import sys
import struct

if len(sys.argv) < 2:
    print("give me a file")
    exit(1)

with open(sys.argv[1], 'wb') as f:
    while True:
        try:
            hdr = sys.stdin.buffer.read(3)
            n = time.time()
            data = sys.stdin.buffer.read(hdr[1])
            f.write(struct.pack("<d", n))
            f.write(hdr)
            f.write(data)
        except:
            f.flush()
            exit(0)
