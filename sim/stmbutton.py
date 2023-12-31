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

shared_data = struct.Struct("HHH")
os.ftruncate(shm_fd, shared_data.size)
# map it

shm_buffer = mmap.mmap(shm_fd, shared_data.size)

print("> mapped data")
curdata = [0, 2048, 2048]
mousedata = None

def update_dat():
    global curdata, mousedata
    if mousedata:
        for i in range(2):
            if mousedata[i] < 0: mousedata[i] = 0
            if mousedata[i] > 4095: mousedata[i] = 4095
        shared_data.pack_into(shm_buffer, 0, curdata[0], *mousedata)
        print("> wrote", curdata[0], *mousedata)
    else:
        shared_data.pack_into(shm_buffer, 0, *curdata)
        print("> wrote", *curdata)

update_dat()

sdl2.ext.init()

window = sdl2.ext.Window("msign button", size=(100, 100))
window.show()

KEYMAP = {
    sdl2.SDLK_p: 0, # power
    sdl2.SDLK_RETURN: 8, # sel
    sdl2.SDLK_m: 7, # menu
    sdl2.SDLK_TAB: 6,
    sdl2.SDLK_s: 5
}

VECTMAP = {
    sdl2.SDLK_UP: [0, -2000],
    sdl2.SDLK_DOWN: [0, 2000],
    sdl2.SDLK_LEFT: [-2000, 0],
    sdl2.SDLK_RIGHT: [2000, 0],
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
                wrt = True
                if event.key.keysym.sym in KEYMAP:
                    curdata[0] |= (1 << KEYMAP[event.key.keysym.sym])
                elif event.key.keysym.sym in VECTMAP:
                    curdata[1] += VECTMAP[event.key.keysym.sym][0]
                    curdata[2] += VECTMAP[event.key.keysym.sym][1]
                else:
                    wrt = False
            elif event.type == sdl2.SDL_KEYUP:
                wrt = True
                if event.key.keysym.sym in KEYMAP:
                    curdata[0] &= ~(1 << KEYMAP[event.key.keysym.sym])
                elif event.key.keysym.sym in VECTMAP:
                    curdata[1] -= VECTMAP[event.key.keysym.sym][0]
                    curdata[2] -= VECTMAP[event.key.keysym.sym][1]
                else:
                    wrt = False
            elif event.type == sdl2.SDL_MOUSEMOTION:
                if mousedata:
                    wrt = True
                    mousedata[0] = int(4095 * (event.motion.x / 100.0))
                    mousedata[1] = int(4095 * (event.motion.y / 100.0))
            elif event.type == sdl2.SDL_MOUSEBUTTONUP:
                if mousedata and event.button.button == sdl2.SDL_BUTTON_LEFT:
                    wrt = True
                    mousedata = None
            elif event.type == sdl2.SDL_MOUSEBUTTONDOWN:
                if not mousedata and event.button.button == sdl2.SDL_BUTTON_LEFT:
                    wrt = True
                    mousedata = [int(4095 * (event.motion.x / 100.0)),
                                 int(4095 * (event.motion.y / 100.0))]
        if wrt:
            update_dat()
        time.sleep(0.01)
finally:
    _posixshmem.shm_unlink("/msign_buttons")
    print("> cleaned up")
