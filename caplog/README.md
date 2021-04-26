# `caplog`

Various tools to dissect capture logs of the communication between the STM and ESP.

## `dissect`

Takes in either a CSV from the Saleae software or a raw `bin` file containing a dump of all bytes transferred, and converts them to a visually
interpretable format.

`caplog/dissect.py [logicexport,simlog] <path to in file>`
(you can also use any prefix of the first argument)

## `regiondump.py`

GDB script that adds a command `dump_bheap` which takes an expression to a raw `bheap::Arena`'s region and a filename and writes out
a summary of all the blocks in the heap.

```
(gdb) source caplog/regiondump.py
(gdb) dump_bheap servicer.arena.region dump_file.txt
```

This also adds another command, `dump_raw_bheap` which outputs a json file (or appends to an existing json file) with an easier
to parse representation of the heap, suitable for visualization with `dumpviewer.html`.

## `dumpviewer.html`

Tiny single page web thing that can read a heap trace and visualize it.
