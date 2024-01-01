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

ignored_dataids = []
ignored_datastr = os.getenv("CAPLOG_IGNORE_IDS")
if ignored_datastr:
    ignored_dataids = [int(x, 16) for x in ignored_datastr.split()]
    print(f"NOTE: ignoring data ids {', '.join(hex(x) for x in ignored_dataids)}")

csum_trailer = 0
if os.getenv("CAPLOG_NO_CSUM"):
    csum_trailer = 0

if len(sys.argv) < 3:
    print("usage: {} [logicexport,simlog,realtime] <in>")
    exit(1)

if sys.argv[1] in prefixes("logicexport"):
    with open(sys.argv[2]) as f:
        f.readline()
        flag = False

        for i in f.readlines():
            i = i.rstrip('\n')
            row = i.split(',')

            if row[2] or row[3]: continue
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
        if ptr > len(datastream):
            exit()
        return bytes(datastream[ptr-x:ptr])

    def read():
        header = bytearray(read_buf(3))
        while header[0] not in (0xa5, 0xa6):
            header[0:1] = header[1:2]
            header[2] = read_buf(1)[0]
        yield bytes(header) + read_buf(header[1]+csum_trailer)
else:
    files = [open(x, 'rb') for x in sys.argv[2:]]
    import select

    def read_exact(f, x):
        buf = bytearray()
        while len(buf) < x:
            buf += os.read(f.fileno(), x-len(buf))
            if len(buf) < x: time.sleep(0.01)
        return buf
    
    def read():
        (ok, _, _) = select.select(files, [], [])
        for f in ok:
            header = read_exact(f, 3)
            while header[0] not in (0xa5, 0xa6):
                print("??? : {:02x}".format(header[0]))
                header[0:2] = header[1:3]
                header[2] = read_exact(f, 1)[0]
            yield bytes(header + read_exact(f, header[1]+csum_trailer))

pnames = {
    0x10: "HANDSHAKE_INIT",
    0x11: "HANDSHAKE_RESP",
    0x12: "HANDSHAKE_OK",
    0x13: "HANDSHAKE_UOK",
    0x20: "DATA_TEMP",
    0x21: "DATA_FULFILL",
    0x22: "DATA_RETRIEVE",
    0x23: "DATA_REQUEST",
    0x24: "DATA_SET_SIZE",
    0x25: "DATA_STORE",
    0x30: "ACK_DATA_TEMP",
    0x31: "ACK_DATA_FULFILL",
    0x32: "ACK_DATA_RETRIEVE",
    0x33: "ACK_DATA_REQUEST",
    0x34: "ACK_DATA_SET_SIZE",
    0x35: "ACK_DATA_STORE",
    0x40: "QUERY_TIME",
    0x50: "RESET",
    0x51: "PING",
    0x52: "PONG",
    0x60: "UPDATE_CMD",
    0x61: "UPDATE_IMG_DATA",
    0x62: "UPDATE_IMG_START",
    0x63: "UPDATE_STATUS",
    0x70: "CONSOLE_MSG",
    0x80: "REFRESH_GRABBER",
    0x81: "SLEEP_ENABLE",
    0x90: "UI_GET_CALIBRATION",
    0x91: "UI_SAVE_CALIBRATION"
}

pktcolors = {
    "HANDSHAKE": 82,
    "QUERY_TIME": 225,
    "PING": 245,
    "PONG": 245,
    "RESET": 226,
    "CONSOLE": 238,
    "DATA_TEMP": 93,
    "DATA_SET_SIZE": 93,
    "DATA_FULFILL": 45,
    "DATA_REQUEST": 101,
    "DATA_STORE": 196,
    "DATA_RETRIEVE": 208,
    "UI_": 166,
}

header_width = 5 + 2 + 2 + 2 + 2 + 20 + 2

tempcodes = {
    0b11: "Hot",
    0b10: "Warm",
    0b01: "ColdWantsWarm",
    0b00: "Cold"
}

slot_temps = {}
slot_databufs = {}

just_restarted = True

def data_temp(dat, from_esp):
    slotid, tempcode = struct.unpack("<HB", bytes(dat))
    
    print(f": request to set {slotid:03x} ({slotlib.slot_types[slotid][0]}) to {tempcodes[tempcode]}")

def ack_data_temp(dat, from_esp):
    slotid, tempcode = struct.unpack("<HB", bytes(dat))
    if tempcode == 0xff:
        print(f": Nack of request to change temp on {slotid:03x} ({slotlib.slot_types[slotid][0]})")
        return
    slot_temps[slotid] = tempcode
    
    print(f": ack of request to set {slotid:03x} ({slotlib.slot_types[slotid][0]}) to {tempcodes[tempcode]}")

def data_update(dat, from_esp, is_move):
    # always from esp
    sid_frame, offs, totalupd = struct.unpack("<HHH", dat[:6])

    start = bool(sid_frame & (1 << 15))
    end = bool(sid_frame & (1 << 14))

    slotid = sid_frame & 0xfff

    endstart = {
            (True, False): "start",
            (False, True): "end",
            (True, True): "whole",
            (False, False): "middle"
    }[start, end]

    if from_esp:
        # patch in
        try:
            slot_databufs[slotid][offs:offs+len(dat)-6] = dat[6:]
        except:
            print(f": data {'move' if is_move else 'update'} [{endstart}] for {slotid:03x} ({slotlib.slot_types[slotid][0]}) @ {offs:04x} (invalid range)")
            return

    print(f": data {'move' if is_move else 'update'} [{endstart}] for {slotid:03x} ({slotlib.slot_types[slotid][0]}) @ {offs:04x}")
    if start:
        print(" "*header_width + f": total update length {totalupd}")
    if end and from_esp:
        if slotid in ignored_dataids:
            print(textwrap.indent("(data supressed due to environment var)", ' '*(header_width + 3)))
            return
        st = slotlib.slot_types[slotid][1]
        if len(slot_databufs[slotid]) >= 512:
            print(textwrap.indent(st.get_formatted(st.parse(slot_databufs[slotid]), (offs + len(dat) - 6 - totalupd), (offs + len(dat) - 6)), ' ' * (header_width + 2) + '└'))
        else:
            print(textwrap.indent(st.get_formatted(st.parse(slot_databufs[slotid])), ' ' * (header_width + 2) + '└'))

updrescode = {
    0: "Ok",
    1: "NotEnoughSpace_TryAgain",
    2: "NotEnoughSpace_Failed",
    0x10: "IllegalState",
    0x11: "NAK",
    0xff: "Timeout"
}

def ack_data_update(dat, from_esp, is_move):
    slotid, upds, updl, code = struct.unpack("<HHHB", dat)

    print(f": ack of data {'move' if is_move else 'update'} for {slotid:03x} ({slotlib.slot_types[slotid][0]}) @ {upds:04x} of length {updl} with code {updrescode[code]}")

def data_chsize(dat, from_esp):
    slotid, totallen = struct.unpack("<HH", dat)

    if slotid not in slot_databufs:
        slot_databufs[slotid] = bytearray(totallen)
    else:
        if len(slot_databufs[slotid]) > totallen:
            slot_databufs[slotid] = slot_databufs[slotid][:totallen]
        else:
            slot_databufs[slotid] += bytearray(totallen - len(slot_databufs[slotid]))
    
    print(f": data size change for {slotid:03x} ({slotlib.slot_types[slotid][0]}) to {totallen} (0x{totallen:04x})")

def ack_data_chsize(dat, from_esp):
    slotid, totallen = struct.unpack("<HH", dat)
    
    print(f": ack of data size change for {slotid:03x} ({slotlib.slot_types[slotid][0]}) to {totallen} (0x{totallen:04x})")

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

def data_request_retrieve(dat, from_esp, is_req):
    slotid, offs, size = struct.unpack("<HHH", dat)

    if is_req:
        print(": request of data update ", end="")
    else:
        print(": request of data retrieval ", end="")
    print(f"for {slotid:03x} ({slotlib.slot_types[slotid][0]}) at {offs:04x} of length {size}")

def ack_data_request_retrieve(dat, from_esp, is_req):
    slotid, offs, size = struct.unpack("<HHH", dat)

    if is_req:
        print(": ack of request for data update ", end="")
    else:
        print(": retrieved data will be sent soon ", end="")

    print(f"for {slotid:03x} ({slotlib.slot_types[slotid][0]}) at {offs:04x} of length {size}")


consolenames = {
    1: "DbgIn",
    2: "DbgOut",
    3: "Log"
}

def console_msg(dat, from_esp):
    idx = dat[0]
    msg = dat[1:].decode("ascii")

    print(f": console msg to {consolenames[idx]}: {msg!r}")

updcmds = {
    0x10: "CancelUpdate",
    0x11: "PepareForImage",
    0x30: "ImgReadError",
    0x31: "ImgCsumError",
    0x32: "InvalidStateError",
    0x40: "UpdateCompletedOk",
    0x50: "EspWroteSector",
    0x51: "EspCopying"
}

updstatuses = {
    0x10: "EnteredUpdMode",
    0x12: "ReadyForImage",
    0x13: "ReadyForChunk",
    0x20: "BeginningCopy",
    0x21: "CopyOk",
    0x30: "ResendLastChunkCsum",
    0x40: "AbortCsum",
    0x41: "AbortProtocol"
}

def update_statcmd(dat, from_esp, typ):
    code = dat[0]
    if typ:
        print(f": {'' if not from_esp else 'unexpected '} update status {updstatuses[code]}")
    else:
        print(f": {'' if from_esp else 'unexpected '} update command {updcmds[code]}")

def update_img_data(dat, from_esp):
    csum = struct.unpack("<H", dat[:2])[0]
    print(f": {len(dat)-2} bytes of image data with checksum {csum:04x}")

def update_img_start(dat, from_esp):
    csum, sz, cnt = struct.unpack("<HIH", dat)
    print(f": image header of length {sz} in {cnt} chunks with checksum {csum:04x} ")

calibstatuses = {
    0x10: "Missing",
    0x11: "MissingIgnore"
}

def format_calibration(dat):
    xm, xM, xd, ym, yM, yd = struct.unpack("<HHHHHH", dat)
    return f"x=(min {xm}, max {xM}, deadzone {xd}), y=(min {ym}, max {yM}, deadzone {yd})"

def ui_get_calibration(dat, from_esp):
    if not from_esp:
        print(f": request for ADC calibration")
    else:
        if dat[0] == 0:
            print(f": ADC calibration with {format_calibration(dat[1:])}")
        else:
            print(f": ADC calibration not present, reason {calibstatuses[dat[0]]}")

def ui_save_calibration(dat, from_esp):
    if from_esp:
        print(f": ack of request to update ADC calibration")
    else:
        print(f": request to save new ADC calibration {format_calibration(dat)}")

phandle = {
    0x20: data_temp,
    0x30: ack_data_temp,
    0x40: query_time,
    0x21: lambda x, y: data_update(x, y, False),
    0x31: lambda x, y: ack_data_update(x, y, False),
    0x25: lambda x, y: data_update(x, y, True),
    0x35: lambda x, y: ack_data_update(x, y, True),
    0x24: data_chsize,
    0x34: ack_data_chsize,
    0x22: lambda x, y: data_request_retrieve(x, y, False),
    0x32: lambda x, y: ack_data_request_retrieve(x, y, False),
    0x23: lambda x, y: data_request_retrieve(x, y, True),
    0x33: lambda x, y: ack_data_request_retrieve(x, y, True),
    0x70: console_msg,
    0x60: lambda x, y: update_statcmd(x, y, False),
    0x63: lambda x, y: update_statcmd(x, y, True),
    0x61: update_img_data,
    0x62: update_img_start,
    0x90: ui_get_calibration,
    0x91: ui_save_calibration
}

last_pkt_time = None

while True:
    for pkt in read():
        if pkt[0] == 0xa5 and pkt[1] == 0x00 and pkt[2] == 0x10 and not just_restarted:
            just_restarted = True
            slot_databufs = {}
            slot_temps = {}
            print("\n\n==== RESTART ====\n\n")
            last_pkt_time = None
        else:
            just_restarted = False
            if last_pkt_time == None:
                last_pkt_time = time.time()
            else:
                now = time.time()
                delta = now - last_pkt_time
                last_pkt_time = now
                if delta > 0.05:
                    if delta < 2:
                        print("    +{}ms".format(int(delta*1000)))
                    else:
                        print("    +{:.02f}s".format(delta))


        if pkt[2] in pnames:
            for j, i in pktcolors.items():
                if j in pnames[pkt[2]]:
                    print('\x1b[38;5;{}m'.format(i), end="")
            if "ACK" in pnames[pkt[2]]:
                print('\x1b[1m', end="")

        if pkt[0] == 0xa6:
            print('E->S ', end='')
        elif pkt[0] == 0xa5:
            print('S->E ', end='')
        else:
            print(' UNK ', end='')

        if pkt[2] not in pnames:
            print(f": {pkt[0]:02x}{pkt[1]:02x}{pkt[2]:02x} <invalid packet>")
            continue

        print(": 0x{:02x} ({:20}) \x1b[0m".format(pkt[2], pnames[pkt[2]]), end="")


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
