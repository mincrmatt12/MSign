import os
import binascii
# dissects capture logs and outputs a list of packets
datastream = []

with open('capturelog.txt') as f:
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

def open_conn(dat):
    if dat[0]:
        print(": C {:02x} as {:04x}".format(dat[1], (dat[2] + dat[3] << 16)))
    else:
        print(": P {:02x} as {:04x}".format(dat[1], (dat[2] + dat[3] << 16)))


def slot_data(dat):
    data = bytes(dat[1:])
    try:
        asc = data.decode('ascii')
    except:
        asc = 'unk'
    
    print(": {:02x} is {} ({})".format(dat[0], binascii.hexlify(data).decode('ascii'), asc))

phandle = {
        0x20: open_conn,
        0x40: slot_data
}

known_slots = {
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
    if header[1] == 0x01:
        v = read(1)[0]
        if header[2] == 0x30:
            known_slots[v] = True
        else:
            known_slots[v] = False

        print(": {:02x}".format(v))
    else:
        if header[2] in phandle:
            phandle[header[2]](read(header[1]))
        else:
            print("? " + binascii.hexlify(bytes(read(header[1]))))

for i in range(256):
    if i in known_slots and known_slots[i]:
        print("slot {:02x} was still open".format(i))
