#!/usr/bin/env python3
import os
import binascii
import sys
import slotlib
import textwrap
import struct
import time
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
    def read_buf(x):
        global ptr
        ptr += x
        if ptr >= len(datastream):
            exit()
        return datastream[ptr-x:ptr]

    def read():
        header = bytearray(read_buf(3))
        while header[0] not in (0xa5, 0xa6):
            header[0:1] = header[1:2]
            header[2] = read_buf(1)[0]
        yield bytes(header) + read_buf(header[1])
else:
    files = [open(x, 'rb') for x in sys.argv[2:]]
    import select

    def read_exact(f, x):
        buf = bytearray()
        while len(buf) < x:
            buf += os.read(f.fileno(), x-len(buf))
        return buf
    
    def read():
        (ok, _, _) = select.select(files, [], [])
        for f in ok:
            header = read_exact(f, 3)
            while header[0] not in (0xa5, 0xa6):
                header[0:1] = header[1:2]
                header[2] = read_exact(f, 1)[0]
            yield bytes(header + read_exact(f, header[1]))

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
    0x41: "QUERY_TIME",
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

slot_temps = {}
slot_databufs = {}

def data_temp(dat, from_esp):
    slotid, tempcode = struct.unpack("<HB", bytes(dat))
    
    print(f": request to set {slotid:03x} ({slotlib.slot_types[slotid][0]}) to {tempcodes[tempcode]}")

phandle = {
    0x20: data_temp
}

def ack_data_temp(dat, from_esp):
    slotid, tempcode = struct.unpack("<HB", bytes(dat))
    
    print(f": acknowledgement of request to set {slotid:03x} ({slotlib.slot_types[slotid][0]}) to {tempcodes[tempcode]}")

def data_update(dat, from_esp):
    pass

timestatus = {
    0: "Ok",
    1: "NotSet"
}

def query_time(dat, from_esp):
    if not from_esp:
        print(f": request")
    else:
        code, t = struct.unpack("<BQ", dat)

        if code == 0:
            print(f": response of time {t} ({time.asctime(time.gmtime(t // 1000))} +{t % 1000}ms)")
        else:
            print(f": response with error code {timestatus[code]}")

phandle = {
    0x20: data_temp,
    0x30: ack_data_temp,
    0x41: query_time
}

while (ptr < len(datastream)) if not realtime else True:
    for pkt in read():
        if pkt[0] == 0xa6:
            print('E->S ', end='')
        elif pkt[0] == 0xa5:
            print('S->E ', end='')
        else:
            print('UNK  ', end='')

        if pkt[2] not in pnames:
            print(f": {pkt[0]:02x}{pkt[1]:02x}{pkt[2]:02x} <invalid packet>")
            continue

        print(": 0x{:02x} ({:20}) ".format(pkt[2], pnames[pkt[2]]), end="")

        if pkt[1] == 0x00:
            print()
            continue
        if pkt[1] == 0x01 and pkt[2] not in phandle:
            print(": {:02x}".format(pkt[3]))
        else:
            if pkt[2] in phandle:
                try:
                    phandle[pkt[2]](pkt[3:], pkt[0] == 0xa6)
                except:
                    print("error decoding " + binascii.hexlify(pkt[3:]).decode("ascii"))
            else:
                print("? " + binascii.hexlify(pkt[3:]).decode("ascii"))
