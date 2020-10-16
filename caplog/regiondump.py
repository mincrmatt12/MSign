import struct
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
import slotlib

# use gdb to generate a report

tempstrs = {
    1: "cold",
    2: "warm",
    3: "hot",
    0: "undefined"
}

locstrs = {
    1: "canonical",
    2: "ephemeral",
    3: "remote",
    0: "undefined"
}

flagstrs = {
    1: "dirty",
    2: "flush"
}

class BHeapDump(gdb.Command):
    def __init__(self):
        super().__init__("dump_bheap", gdb.COMMAND_DATA, gdb.COMPLETE_EXPRESSION)

    def invoke(self, arg, from_tty):
        args = gdb.string_to_argv(arg)
        region_blob = gdb.parse_and_eval(args[0])
        fname = args[1]

        print("got blob ", region_blob, region_blob.address, region_blob.type.sizeof)
        
        memarr = gdb.selected_inferior().read_memory(region_blob.address, region_blob.type.sizeof)

        print("got memarr", memarr)

        pos = 0

        slotoffdb = {}

        with open(fname, "w") as f:
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
                    
                print(f"- <{slotid:03x}> / {datasize} / {tempstrs[temperature]} / {locstrs[location]} / ({flagstr})", file=f)

                slotcontents_pos = pos
                if location != 3:
                    pos += rounded_datasize
                
                if slotid == 4081:
                    print("  End", file=f)
                    break
                elif slotid == 4080:
                    print(f"  Empty", file=f)
                    continue
                else:
                    if slotid not in slotoffdb:
                        slotoffdb[slotid] = datasize
                        act_off = 0
                    else:
                        act_off = slotoffdb[slotid]
                        slotoffdb[slotid] += datasize
                    print(f"  {slotlib.slot_types[slotid][0]} @ {act_off:03x}", file=f)

BHeapDump()        
