"""
This script creates the certs.bin file that can be sent to the webui to update ca certificates.

The file has a very basic format. The first four bytes are 'MSCA' in ASCII as a header. This is then followed
by a 4-byte number of certificate records, then the records themselves. Each record is made up of:

- 64 byte file name, again in ascii (the sha256 of the subject cn)
- 4 byte file size
- raw contents of file

This should be run after generate_sd is called, passing the `ca/` directory on the generated sd.
"""

import pathlib
import struct
import sys

if len(sys.argv) != 3:
    print("usage: {} build/ca certs.bin".format(sys.argv[0]))
    exit(1)

_, in_dir, out_path = sys.argv

file_records = []

for name in pathlib.Path(in_dir).iterdir():
    if name.is_file():
        if len(str(name.name)) != 64:
            print("skipping {}".format(name))
            continue
        with open(name, "rb") as f:
            file_records.append((str(name.name), f.read()))

with open(out_path, "wb") as f:
    f.write(b"MSCA")
    f.write(struct.pack("<I", len(file_records)))
    for name, content in file_records:
        f.write(name.encode("ascii"))
        f.write(struct.pack("<I", len(content)))
        f.write(content)
