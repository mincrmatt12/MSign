"""
Low-effort data storage
"""

import os
import json
import tempfile
import unix_ar
import shutil
import tinydb

from . import image

DATAVOL_PATH = os.getenv("MSIGN_DATA_PATH", "/data")
DATAVOL_WEBUI = os.path.join(DATAVOL_PATH, "web")
DATAVOL_FILE = os.path.join(DATAVOL_PATH, "file")

if not os.path.exists(DATAVOL_WEBUI):
    os.mkdir(DATAVOL_WEBUI)

if not os.path.exists(DATAVOL_FILE):
    os.mkdir(DATAVOL_FILE)

def get_webui_file(path: str):
    """
    Return a path to the requested webui file
    """

    return os.path.join(DATAVOL_WEBUI, path)

def update_webui_archive(upload):
    """
    Install a new webui archive from f, a flask upload
    """

    FILES = ("page.html", "page.css", "page.js")
    archive = unix_ar.open(upload, "r")

    with tempfile.TemporaryDirectory() as tmpdir:
        for name in FILES:
            archive.extract(name.encode("ascii") + b"/", os.path.join(tmpdir, name).encode("utf-8"))
        
        for name in FILES:
            shutil.copy(os.path.join(tmpdir, name), os.path.join(DATAVOL_WEBUI, name))

# Load settings:
with open(os.getenv("MSIGN_SHARED_SECRET"), "rb") as f:
    shared_secret = f.read()

with open(os.getenv("MSIGN_SERVER_SECRET"), "rb") as f:
    server_secret = f.read()

db = tinydb.TinyDB(os.path.join(DATAVOL_PATH, "db.json"))

users = db.table("user")
bearer_tokens = db.table("tokens")

def get_served_file(path: str):
    """
    Get a config/firmware file
    """

    return os.path.join(DATAVOL_FILE, path)

def update_firm(stm_upload, esp_upload):
    with tempfile.TemporaryDirectory() as tmpdir:
        stmpath = os.path.join(tmpdir, "stm.bin")
        esppath = os.path.join(tmpdir, "esp.bin")

        stm_upload.save(stmpath)
        esp_upload.save(esppath)

        with open(esppath, "rb") as f:
            vernum, verproj = image.get_esp_image_description(f.read())

        shutil.copy(stmpath, get_served_file("stm.bin"))
        shutil.copy(esppath, get_served_file("esp.bin"))

        update_stored_version("firm", vernum)

def get_served_versions():
    """
    Get the versions json
    """

    try:
        with open(os.path.join(DATAVOL_PATH, "versions.json")) as f:
            return json.load(f)
    except FileNotFoundError:
        return {}

def update_stored_version(name: str, value):
    current_vertable = get_served_versions()
    current_vertable[name] = value
    with open(os.path.join(DATAVOL_PATH, "versions.json"), "w") as f:
        json.dump(current_vertable, f)
