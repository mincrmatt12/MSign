#!/usr/bin/env python3
import os
import binascii
import sys
import slotlib
import textwrap
# dissects capture logs and outputs a list of packets
datastream = []
prefixes = lambda y: [y[:x] for x in range(1, len(y)+1)]

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
else:
    print("usage: {} [logicexport,simlog] <in>")
    exit(1)

ptr = 0
def read(x):
    global ptr
    ptr += x
    return datastream[ptr-x:ptr]

pnames = {
    0x10: "HANDSHAKE_INIT",
    0x11: "HANDSHAKE_RESP",
    0x12: "HANDSHAKE_OK",
    0x13: "HANDSHAKE_UOK",
    0x20: "OPEN_CONN",
    0x21: "CLOSE_CONN",
    0x22: "NEW_DATA",
    0x30: "ACK_OPEN_CONN",
    0x31: "ACK_CLOSE_CONN",
    0x40: "SLOT_DATA",
    0x50: "RESET",
    0x51: "PING",
    0x52: "PONG",
    0x60: "UPDATE_CMD",
    0x61: "UPDATE_IMG_DATA",
    0x62: "UPDATE_IMG_START",
    0x63: "UPDATE_STATUS"
}

header_width = 5 + 2 + 2 + 2 + 2 + 20 + 2
connection_data = {
    
}

def open_conn(dat):
    connection_data[dat[1]] = dat[2] + (dat[3] << 8)
    if not dat[0]:
        print(": continuous {:02x} as {:04x} (i.e. {})".format(dat[1], connection_data[dat[1]], slotlib.slot_types[connection_data[dat[1]]][0]))
    else:
        print(": polled {:02x} as {:04x} (i.e. {})".format(dat[1], connection_data[dat[1]], slotlib.slot_types[connection_data[dat[1]]][0]))

def close_conn(dat):
    print(": {:02x}, was {:04x} (i.e. {})".format(dat[0], connection_data[dat[0]], slotlib.slot_types[connection_data[dat[0]]][0]))
    del connection_data[dat[0]]

def slot_data(dat):
    data = bytes(dat[1:])
    sid = dat[0]
    if sid not in connection_data:
        try:
            asc = data.decode('ascii')
        except:
            asc = 'unk'
        
        print(": {:02x} is {} ({})".format(sid, binascii.hexlify(data).decode('ascii'), asc))
    else:
        print(": {:02x} ({:04x}, i.e. {}) is:".format(sid, connection_data[sid], slotlib.slot_types[connection_data[sid]][0]))

        st = slotlib.slot_types[connection_data[sid]][1]
        print(textwrap.indent(st.get_formatted(st.parse(data)), ' ' * (header_width + 2) + '└'))

phandle = {
        0x20: open_conn,
        0x21: close_conn,
        0x40: slot_data
}

while ptr < len(datastream):
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
            print("? " + binascii.hexlify(bytes(read(header[1]))))
