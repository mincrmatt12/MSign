#!/usr/bin/env python3
import sys
import struct

if len(sys.argv) != 4:
    print(f"usage: {sys.argv[0]} <in.obj> <in.mtl> <out.bin>")
    exit(1)

verticies = []
faces = []
materials = {}

with open(sys.argv[2]) as f:
    name = ""
    datr = None
    for line in f:
        dat = line[:-1].split(" ")
        if dat[0] == "newmtl":
            name = dat[1]
        elif dat[0] == "Kd":
            datr = [float(x) * 255 for x in dat[1:]]
            materials[name] = datr


with open(sys.argv[1]) as f:
    cmat = [255, 255, 255]
    for line in f:
        dat = line[:-1].split(" ")
        if dat[0] == "v":
            verticies.append([float(x) for x in dat[1:]])
        elif dat[0] == "usemtl":
            cmat = materials[dat[1]]
        elif dat[0] == "f":
            face = []
            for i in dat[1:]:
                b = int(i.split("/")[0]) - 1
                face.append(verticies[b])
            faces.append(face + cmat)

with open(sys.argv[3], 'wb') as f:
    f.write(struct.pack("<H", len(faces)))
    for face in faces:
        f.write(struct.pack("<3f", *face[0]))
        f.write(struct.pack("<3f", *face[1]))
        f.write(struct.pack("<3f", *face[2]))
        f.write(struct.pack("<3f", face[3], face[4], face[5]))

