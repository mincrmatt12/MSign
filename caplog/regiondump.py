import struct
import sys
import os
import json
import base64
import time

sys.path.insert(0, os.path.dirname(__file__))
import slotlib

# use gdb to generate a report

tempstrs = {
    0: "cold",
    1: "coldwarm",
    2: "warm",
    3: "hot"
}

locstrs = {
    1: "canonical",
    2: "ephemeral",
    3: "remote",
    0: "undefined"
}

flagstrs = {
    1: "dirty",
    2: "unused"
}

class RawBHeapDump(gdb.Command):
    def __init__(self):
        super().__init__("dump_raw_bheap", gdb.COMMAND_DATA, gdb.COMPLETE_EXPRESSION)

    def invoke(self, arg, from_tty):
        args = gdb.string_to_argv(arg)
        region_blob = gdb.parse_and_eval(args[0])
        fname = args[1]
        if len(args) > 2:
            print(args)
            try:
                msg = gdb.execute(args[2], to_string=True)
            except:
                msg = "<eval fail>"
        else:
            msg = ""

        #print("got blob ", region_blob, region_blob.address, region_blob.type.sizeof)
        
        memarr = gdb.selected_inferior().read_memory(region_blob.address, region_blob.type.sizeof)

        #print("got memarr", memarr)

        if not os.path.exists(fname):
            data = {
                "size": len(memarr),
                "records": []
            }
        else:
            try:
                with open(fname, "r") as f:
                    data = json.load(f)
            except json.JSONDecodeError:
                data = {
                    "size": len(memarr),
                    "records": []
                }


        if data["size"] != len(memarr):
            print("invalid size")
            return

        pos = 0

        slotoffdb = {}

        record = []

        while pos < region_blob.type.sizeof:
            hdr = struct.unpack_from("<I", memarr, pos)[0]
            slotid = hdr & 0b111111111111
            datasize = (hdr >> 12) & 0b11111111111111
            temperature = (hdr >> (12+14)) & 0b11
            location = (hdr >> (12+14+2)) & 0b11
            flags = (hdr >> (12+14+2+2)) & 0b11
            pos += 4

            rounded_datasize = datasize if datasize % 4 == 0 else datasize + (4 - (datasize % 4))
            flagstr = ", ".join(flagstrs[x] for x in (
                flags & 1, flags & 2
            ) if x)

            blkdat = {
                "raw": hdr,
                "slotid": slotid,
                "datasize": datasize,
                "temperature": tempstrs[temperature],
                "location": locstrs[location] if location else None,
                "flags": [
                    flags & 1,
                    flags & 2
                ],
            }

            if location != 3 and slotid < 4080:
                blkdat["contents"] = None if location == 3 or slotid >= 4080 else base64.b64encode(memarr[pos:pos+datasize]).decode('ascii')

            slotcontents_pos = pos
            if location != 3:
                pos += rounded_datasize
            
            if slotid == 4081:
                blkdat["slotname"] = "End"
                record.append(blkdat)
                break
            elif slotid == 4080:
                blkdat["slotname"] = "Empty"
                record.append(blkdat)
                continue
            else:
                if slotid not in slotoffdb:
                    slotoffdb[slotid] = datasize
                    act_off = 0
                else:
                    act_off = slotoffdb[slotid]
                    slotoffdb[slotid] += datasize
                blkdat["slotname"] = slotlib.slot_types[slotid][0] if slotid in slotlib.slot_types else "<unk>"
                blkdat["slotoffset"] = act_off
            record.append(blkdat)

        data["records"].append({
            "blocks": record,
            "time": time.time(),
            "msg": msg
        })
        with open(fname, "w") as f:
            f.write(json.dumps(data))


class BHeapDump(gdb.Command):
    def __init__(self):
        super().__init__("dump_bheap", gdb.COMMAND_DATA, gdb.COMPLETE_EXPRESSION)

    def invoke(self, arg, from_tty):
        args = gdb.string_to_argv(arg)
        region_blob = gdb.parse_and_eval(args[0])
        fname = args[1]

        memarr = gdb.selected_inferior().read_memory(region_blob.address, region_blob.type.sizeof)

        pos = 0

        slotoffdb = {}

        while pos < region_blob.type.sizeof:
            hdr = struct.unpack_from("<I", memarr, pos)[0]
            slotid = hdr & 0b111111111111
            datasize = (hdr >> 12) & 0b11111111111111
            temperature = (hdr >> (12+14)) & 0b11
            location = (hdr >> (12+14+2)) & 0b11
            flags = (hdr >> (12+14+2+2)) & 0b11
            pos += 4

            rounded_datasize = datasize if datasize % 4 == 0 else datasize + (4 - (datasize % 4))
            flagstr = ", ".join(flagstrs[x] for x in (
                flags & 1, flags & 2
            ) if x)
                
            print(f"- <{slotid:03x}> / {datasize} / {tempstrs[temperature]} / {locstrs[location]} / ({flagstr})")

            slotcontents_pos = pos
            if location != 3:
                pos += rounded_datasize
            
            if slotid == 4081:
                print("  End")
                break
            elif slotid == 4080:
                print(f"  Empty")
                continue
            else:
                if slotid not in slotoffdb:
                    slotoffdb[slotid] = datasize
                    act_off = 0
                else:
                    act_off = slotoffdb[slotid]
                    slotoffdb[slotid] += datasize
                print(f"  {slotlib.slot_types[slotid][0] if slotid in slotlib.slot_types else '<unrecognized>'} @ {act_off:03x}")

BHeapDump()        
RawBHeapDump()
