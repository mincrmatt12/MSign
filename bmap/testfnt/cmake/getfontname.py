import sys
import os
import string

_, face_name, size_pixels = sys.argv
size_pixels = int(size_pixels)
print(os.path.splitext(os.path.basename(face_name))[0].replace("-", "_").replace(" ", "_").strip(string.digits + string.whitespace + "./\"'!@#$%^&*()[]{};,.").lower() + "_" + str(size_pixels))
