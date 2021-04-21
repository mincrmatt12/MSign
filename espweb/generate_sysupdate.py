"""
Takes two bin files and prepares various checksum files for them
"""

import sys
import os
import struct
import shutil

if len(sys.argv) != 4:
    print("usage: {} stm.bin esp.bin target_upd_folder/".format(sys.argv[0]))
    exit(1)

_, stm, esp, target = sys.argv

shutil.copy(stm, os.path.join(target, "stm.bin"))
shutil.copy(esp, os.path.join(target, "esp.bin"))

def crc(fname):
    crc = 0
    with open(fname, "rb") as f:
        for k in f:
            for i in k:
                crc ^= i
                for j in range(8, -1, -1):
                    crc = (crc >> 1) ^ (0x8005 if (crc & 1) else 0)
    return crc

with open(os.path.join(target, "chck.sum"), "wb") as f:
    f.write(struct.pack("<HH", crc(esp), crc(stm)))

with open(os.path.join(target, "state"), "wb") as f:
    f.write(bytes([0]))
