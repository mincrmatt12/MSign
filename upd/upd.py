#!/usr/bin/env python3.6

import sys
import socket
import struct
import time

def usage():
    print("Usage: upd {reset,download,upload} [file]")
    print("Usage: upd {update} esp stm")

if len(sys.argv) < 2:
    usage()
    exit(1)

command = sys.argv[1]

s = socket.socket()
s.connect(("msign.mm12.xyz", 34476))

s.send(b"Mn")

def reset():
    s.send(b"\x10")
    print("Reset target")

def download():
    if len(sys.argv) < 3:
        usage()
        s.close()
    else:
        s.send(b"\x20")
        print("Downloading conf")
        with open(sys.argv[2], "w") as f:
            while True:
                a = s.recv(1)
                if a == b"\x00":
                    break
                else:
                    f.write(a.decode("ascii"))
                    print(a.decode("ascii"), end="")
        s.close()

def upload():
    if len(sys.argv) < 3:
        usage()
        s.close()
    else:
        with open(sys.argv[2], "r") as f:
            text = f.read()
        ll = len(text)
        print("Uploading len {} conf".format(ll))

        s.send(b"\x30")

        s.send(struct.pack("<I", ll))
        time.sleep(0.05)
        s.send(text.encode("ascii"))

        code = s.recv(1)

        if code == b"\x00":
            print("OK")
        elif code == b"\x10":
            print("Too big")
        elif code == b"\x11":
            print("SD card write failed")
        else:
            print("Unknown error")


def update():
    if len(sys.argv) < 4:
        usage()
    else:
        import requests
        from getpass import getpass

        username = raw_input("user: ")
        password = getpass()

        with open(sys.argv[2], 'rb') as esp:
            with open(sys.argv[3], 'rb') as stm:
                requests.post('http://msign.mm12.xyz/a/updatefirm', auth=requests.auth.HTTPDigestAuth(username, password), files={
                    'stm': stm,
                    'esp': esp
                })
        
        print('did something')

cc = {
    "reset": reset,
    "download": download,
    "upload": upload
}

if command not in cc:
    usage()
else:
    cc[command]()
    time.sleep(1)
    s.close()
