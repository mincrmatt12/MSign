#!/usr/bin/env python3
import shutil
import sys
import os

# usage:
# source, target, overlay

del sys.argv[0]

if not os.path.exists(sys.argv[1]):
    os.makedirs(sys.argv[1])

for root, dirs, files in os.walk(sys.argv[0], followlinks=True):
    root = os.path.relpath(root, sys.argv[0])
    # check if dirs exist in target
    for d in dirs:
        if not os.path.exists(os.path.join(sys.argv[1], root, d)):
            os.mkdir(os.path.join(sys.argv[1], root, d))

    # for each file ...
    for f in files:
        # copy the files preserving attributes
        if os.path.exists(os.path.join(sys.argv[2], root, f)):
            shutil.copy2(os.path.join(sys.argv[2], root, f), os.path.join(sys.argv[1], root, f))
        else:
            shutil.copy2(os.path.join(sys.argv[0], root, f), os.path.join(sys.argv[1], root, f))

# copy all overlay files not in the directories
for root, dirs, files in os.walk(sys.argv[2], followlinks=True):
    root = os.path.relpath(root, sys.argv[2])

    for f in files:
        if not os.path.exists(os.path.join(sys.argv[0], root, f)):
            shutil.copy2(os.path.join(sys.argv[2], root, f), os.path.join(sys.argv[1], root, f))
