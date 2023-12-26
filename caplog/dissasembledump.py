import json
import slotlib
import sys
import base64

if len(sys.argv) != 3:
    print("usage: dissasembledump <dump.json> <index>")

contents_map = {}

with open(sys.argv[1], "r") as f:
    blocks = json.load(f)

for block in blocks["records"][int(sys.argv[2])]["blocks"]:
    if "contents" not in block:
        continue
    if block["location"] == "remote":
        contents_map[block["slotid"]] = None
        continue
    if block["slotid"] in contents_map:
        contents_map[block["slotid"]].extend(base64.b64decode(block["contents"]))
    else:
        contents_map[block["slotid"]] = bytearray(base64.b64decode(block["contents"]))

for key in sorted(contents_map):
    raw = contents_map[key]
    print(f"< {slotlib.slot_types[key][0]} >")
    if raw is None:
        print(f"... is incomplete in dump")
    else:
        st = slotlib.slot_types[key][1]
        try:
            print(st.get_formatted(st.parse(raw)))
        except:
            print(f"... cannot be decoded")
