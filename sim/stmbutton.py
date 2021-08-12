#!/usr/bin/env python3

import os
import mmap
import ctypes
import struct
import stat
import sys
import sdl2
import sdl2.ext

rtld = ctypes.PyDLL("librt.so", use_errno=True)

# Create shared memory

shm_fd = rtld.shm_open(ctypes.create_string_buffer(b"/msign_buttons"), ctypes.c_int(os.O_RDWR | os.O_CREAT | os.O_EXCL), ctypes.c_ushort(0o666))

if shm_fd == -1:
    raise RuntimeError(os.strerror(ctypes.get_errno()))

print("> created shm fd")

shared_data = struct.Struct("H")
os.ftruncate(shm_fd, shared_data.size)
# map it

shm_buffer = mmap.mmap(shm_fd, shared_data.size)

print("> mapped data")
curdata = [0]

def update_dat():
    global curdata
    shared_data.pack_into(shm_buffer, 0, *curdata)
    print("> wrote", *curdata)

update_dat()

sdl2.ext.init()

window = sdl2.ext.Window("msign button", size=(100, 100))
window.show()

KEYMAP = {
    sdl2.SDLK_p: 0, # power
    sdl2.SDLK_RIGHT: 10, # nxt
    sdl2.SDLK_LEFT: 9, # prv
    sdl2.SDLK_RETURN: 8, # sel
    sdl2.SDLK_m: 7, # menu
}

run = True
try:
    while run:
        events = sdl2.ext.get_events()
        wrt = False
        for event in events:
            if event.type == sdl2.SDL_QUIT:
                run = False
                break
            elif event.type == sdl2.SDL_KEYDOWN:
                if event.key.repeat:
                    continue
                if event.key.keysym.sym not in KEYMAP:
                    continue
                curdata[0] |= (1 << KEYMAP[event.key.keysym.sym])
                wrt = True
            elif event.type == sdl2.SDL_KEYUP:
                if event.key.keysym.sym not in KEYMAP:
                    continue
                curdata[0] &= ~(1 << KEYMAP[event.key.keysym.sym])
                wrt = True
        if wrt:
            update_dat()
finally:
    rtld.shm_unlink(ctypes.create_string_buffer(b"/msign_buttons"))
    print("> cleaned up")
