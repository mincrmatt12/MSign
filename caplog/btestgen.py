# Generates a test case array (just the contents of "region") from a raw dump.

import json
import sys
import struct
import base64

with open(sys.argv[1]) as f:
    datas = json.load(f)

relevant_record = datas["records"][int(sys.argv[2])]
array = bytearray(datas["size"])

tempstrs = {
    "coldwarm": 1,
    "cold": 0,
    "warm": 2,
    "hot": 3,
    None: 0
}

locstrs = {
    "canonical": 1,
    "ephemeral": 2,
    "remote": 3,
    "undefined": 0,
    None: 0
}

pos = 0
for record in relevant_record["blocks"]:
    # reconstruct header
    hdr = (
        record["slotid"] |
        (record["datasize"] << 12) |
        (tempstrs[record["temperature"]] << (12+14)) |
        (locstrs[record["location"]] << (12+14+2)) |
        (record["flags"][0] << (12+14+2+2)) |
        (record["flags"][1] << (12+14+2+2))
    )
    try:
        struct.pack_into("<I", array, pos, hdr)
    except struct.error:
        print("invalid record:", record, hdr)
        raise
    pos += 4
    # write data
    if "contents" in record:
        array[pos:pos+record["datasize"]] = base64.b64decode(record["contents"])
    # advance space
    if record["location"] != "remote":
        pos += record["datasize"]
        if pos % 4:
            pos += (4 - (pos % 4))

# Generate c-compatible array declaration
print("const uint8_t testcase[] = {")
print("    ", end="");
for i in array:
    print("0x{:02x},".format(i), end="")
print()
print("};")
