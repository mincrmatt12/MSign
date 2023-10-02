#!/usr/bin/env python3

import os
import mmap
import time
import struct
import stat
import sys
import sdl2
import sdl2.ext
import _posixshmem

# Create shared memory

shm_fd = _posixshmem.shm_open("/msign_buttons", os.O_RDWR | os.O_CREAT | os.O_EXCL, 0o666)

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
        time.sleep(0.01)
finally:
    _posixshmem.shm_unlink("/msign_buttons")
    print("> cleaned up")
