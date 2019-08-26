#!/usr/bin/env python3
import struct
import sys

queues = []

for i in sys.argv[1:]:
    queue = []
    with open(i, 'rb') as f:
        idx = 0
        data = f.read()
        def read_data(amt):
            global idx
            x = data[idx:idx+amt]
            idx += amt
            return x

        while idx < len(data):
            tm = read_data(8)
            hdr = read_data(3)
            packdat = read_data(hdr[1])

            queue.append((struct.unpack("<d", tm)[0], hdr + packdat))
    queues.append(queue)

while sum(len(x) for x in queues) > 0:
    heads = []
    for j, i in enumerate(queues):
        if len(i) > 0:
            heads.append((i[0], j))
    
    next_value = min(heads, key=lambda x: x[0][0])
    del queues[next_value[1]][0]

    sys.stdout.buffer.write(next_value[0][1])
