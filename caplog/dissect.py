#!/usr/bin/env python3
import os
import binascii
import sys
import slotlib
import textwrap
import struct
# dissects capture logs and outputs a list of packets
datastream = []
prefixes = lambda y: [y[:x] for x in range(1, len(y)+1)]
realtime = False

if len(sys.argv) < 3:
    print("usage: {} [logicexport,simlog] <in>")
    exit(1)

if sys.argv[1] in prefixes("logicexport"):
    with open(sys.argv[2]) as f:
        f.readline()
        flag = False

        for i in f.readlines():
            i = i.rstrip('\n')
            row = i.split(',')

            v = int(row[1][2:], base=16)
            if not flag and v not in (0xa6, 0xa5):
                continue
            flag = True
            datastream.append(v)
elif sys.argv[1] in prefixes("simlog"):
    with open(sys.argv[2], 'rb') as f:
        datastream = [x for x in f.read()]
elif sys.argv[1] in prefixes("realtime"):
    realtime = True
else:
    print("usage: {} [logicexport,simlog,realtime] <in>")
    exit(1)

ptr = 0
if not realtime:
    def read(x):
        global ptr
        ptr += x
        return datastream[ptr-x:ptr]
else:
    files = [open(x, 'rb') for x in sys.argv[2:]]
    import select
    
    def read(x):
        if len(datastream) >= x:
            return [datastream.pop(0) for y in range(x)]
        else:
            while len(datastream) < x:
                (ok, _, _) = select.select(files, [], [])
                for f in ok:
                    datastream.extend([y for y in os.read(f.fileno(), 128)])
            return read(x)


pnames = {
    0x10: "HANDSHAKE_INIT",
    0x11: "HANDSHAKE_RESP",
    0x12: "HANDSHAKE_OK",
    0x13: "HANDSHAKE_UOK",
    0x20: "DATA_TEMP",
    0x21: "DATA_UPDATE",
    0x22: "DATA_MOVE",
    0x23: "DATA_DEL",
    0x30: "ACK_DATA_TEMP",
    0x31: "ACK_DATA_UPDATE",
    0x32: "ACK_DATA_MOVE",
    0x33: "ACK_DATA_DEL",
    0x40: "QUERY_FREE_HEAP",
    0x50: "RESET",
    0x51: "PING",
    0x52: "PONG",
    0x60: "UPDATE_CMD",
    0x61: "UPDATE_IMG_DATA",
    0x62: "UPDATE_IMG_START",
    0x63: "UPDATE_STATUS",
    0x70: "CONSOLE_MSG"
}

header_width = 5 + 2 + 2 + 2 + 2 + 20 + 2

tempcodes = {
    0b11: "Hot",
    0b10: "Warm",
    0b01: "Cold"
}

def data_temp(dat):
    slotid, tempcode = struct.unpack("<HB", bytes(dat))
    
    print(f" : request to set {slotid:03x} ({slotlib.slot_types[slotid][0]}) to {tempcodes[tempcode]}")

phandle = {
    0x20: data_temp
}

while (ptr < len(datastream)) if not realtime else True:
    header = read(3)

    if header[0] == 0xa6:
        print('E->S ', end='')
    elif header[0] == 0xa5:
        print('S->E ', end='')
    else:
        print('UNK  ', end='')

    print(": 0x{:02x} ({:20}) ".format(header[2], pnames[header[2]]), end="")

    if header[1] == 0x00:
        print()
        continue
    if header[1] == 0x01 and header[2] not in phandle:
        v = read(1)[0]
        print(": {:02x}".format(v))
    else:
        if header[2] in phandle:
            phandle[header[2]](read(header[1]))
        else:
            print("? " + binascii.hexlify(bytes(read(header[1]))).decode("ascii"))
