import struct
import random

def get_esp_image_description(image_data: bytes):
    """
    Attempt to determine the version information for a given esp binary image.

    Returns (project_version, project_name) or raises a ValueError
    """

    magic, num_segments = struct.unpack_from("<BB", image_data, 0)
    if magic != 0xe9:
        raise ValueError("not an esp image")

    base = 8
    for i in range(num_segments):
        memoff, size = struct.unpack_from("<II", image_data, base) 
        base += 8 
        magic = struct.unpack_from("<I", image_data, base)[0] 
        if magic == 0xabcd5432: 
            magic, secversion, project_version, project_name, copmile_time, compile_date, idf_version = struct.unpack_from("<II8x32s32s16s16s32s", image_data, base)
            project_version = project_version[:project_version.index(0)].decode('ascii')
            project_name = project_name[:project_name.index(0)].decode('ascii')

            return (project_version, project_name)
     
        base += size 
    raise ValueError("could not find an app description header")

def get_config_version(config_blob: dict):
    """
    Get the version of a config file
    """

    if "cfgpull" not in config_blob:
        return 0
    elif "version" not in config_blob["cfgpull"]:
        return 0
    return config_blob["cfgpull"]["version"]

def set_config_version_on(config_blob: dict):
    if "cfgpull" not in config_blob:
        config_blob["cfgpull"] = {}
    config_blob["cfgpull"]["version"] = random.randint(0, 2147483647)
