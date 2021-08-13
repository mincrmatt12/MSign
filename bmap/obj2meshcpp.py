#!/usr/bin/env python3
import sys
import struct

if len(sys.argv) != 4:
    print(f"usage: {sys.argv[0]} <in.obj> <in.mtl> <out.h>")
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

def fixed_str(x):
    value = int(round(x * 32768));
    return f"m::fixed_t{{{value}, nullptr}}"

with open(sys.argv[3], 'w') as f:
    print("face count: ", len(faces))

    print(f"// count = {len(faces)}", file=f)
    print("#include \"../threed.h\"", file=f)
    print(file=f)
    print("namespace threed {", file=f)
    print(file=f)
    print("struct DefaultMeshHolder {", file=f)
    print(f"Tri tris[{len(faces)}];", file=f)
    print(f"const static inline size_t tri_count = {len(faces)};", file=f)
    print(file=f)
    print("constexpr DefaultMeshHolder() : tris{} {", file=f)

    for j, face in enumerate(faces):
        print(f"""tris[{j}] = {{
    {{ {fixed_str(face[0][0])}, {fixed_str(face[0][1])}, {fixed_str(face[0][2])} }},
    {{ {fixed_str(face[1][0])}, {fixed_str(face[1][1])}, {fixed_str(face[1][2])} }},
    {{ {fixed_str(face[2][0])}, {fixed_str(face[2][1])}, {fixed_str(face[2][2])} }},
    {int(face[3])},
    {int(face[4])},
    {int(face[5])}
}};\n""", file=f)

    print("} }; }", file=f)

