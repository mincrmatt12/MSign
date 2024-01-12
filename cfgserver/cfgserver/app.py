from . import db, image, auth

import qrcode
import os
from flask import Flask, request, redirect, send_file, abort, Response
from werkzeug.exceptions import Unauthorized
from werkzeug.routing import BaseConverter
from flask_login import current_user
import tempfile
import json
import click
import time

class OnlyAlphanumeric(BaseConverter):
    def __init__(self, url_map):
        super().__init__(url_map)
        self.regex = "[a-zA-Z0-9]+"

lowfi_login_page = r"""
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>msign remote login</title>
<link rel="stylesheet" href="/page.css" />
</head>
<body>
<div class="container">
    <form class="py-3" action="/login" method="post" enctype="application/x-ww-form-urlencoded">
        <div class="mb-2">
            <label for="userIn" class="form-label">username</label>
            <input type="text" class="form-control" id="userIn" name="user">
        </div>
        <div class="mb-2">
            <label for="passwordIn" class="form-label">password</label>
            <input type="password" class="form-control" id="passwordIn" name="password">
        </div>
        <div class="mb-2">
            <label for="twofaIn" class="form-label">2fa code</label>
            <input type="text" class="form-control" id="twofaIn" name="twofa">
        </div>
        <button type="submit" class="btn btn-success">login</button>
    </form>
</div>
</body>
</html>
"""

metric_doc = {
    "mem_free": ("Total memory free on ESP", "guage"),
    "mem_free_dram": ("Total DRAM free on ESP", "guage"),
    "bheap_space": ("Empty space in bheap", "guage")
}

metric_table = {}
metric_last_sent = 0

app = Flask(__name__)
app.url_map.converters['alphanum'] = OnlyAlphanumeric

@app.errorhandler(FileNotFoundError)
def handle_bad_file(e):
    abort(404)

@app.route("/a/metrics")
@auth.priv_required(auth.Priv.READ)
def serve_metrics():
    curtime = time.monotonic()

    if curtime - metric_last_sent > 15*60:
        return "host stopped sending reports", 404

    data = """# msign metric data\n"""
    for nm, ndat in metric_table.items():
        real_name = "msign_" + nm
        if nm in metric_doc:
            data += f"# HELP {real_name} {metric_doc[nm][0]}\n#TYPE {real_name} {metric_doc[nm][1]}\n"
        data += "msign_{} {}\n".format(nm, ndat)
    return data

@app.route("/login", methods=["GET", "POST"])
def show_or_login():
    if request.method == "GET":
        return lowfi_login_page
    else:
        if auth.try_login_user(request.form["user"], request.form["password"], request.form["twofa"]):
            return redirect("/")
        else:
            raise Unauthorized()

@app.route("/")
@app.route("/<alphanum:_>")
def root(_=None):
    if current_user.is_anonymous:
        return redirect("/login")
    
    return send_file(db.get_webui_file("page.html"), mimetype="text/html")

@app.route("/page.css")
@app.route("/page.<alphanum:_>.css")
def send_css(_=None):
    return send_file(db.get_webui_file("page.css"), mimetype="text/css")

@app.route("/page.js")
@app.route("/page.<alphanum:_>.js")
@auth.priv_required(auth.Priv.READ)
def send_js(_=None):
    return send_file(db.get_webui_file("page.js"), mimetype="application/javascript")

@app.route("/vendor.js")
@app.route("/vendor.<alphanum:_>.js")
@auth.priv_required(auth.Priv.READ)
def send_vjs(_=None):
    return send_file(db.get_webui_file("vendor.js"), mimetype="application/javascript")

@app.route("/a/conf.json")
@auth.priv_required(auth.Priv.READ)
def get_config():
    return send_file(db.get_served_file("conf.json"), mimetype="application/json")

@app.route("/a/conf.json", methods=["POST"])
@auth.priv_required(auth.Priv.WRITE)
def store_config():
    newdata = json.loads(request.data)
    image.set_config_version_on(newdata)
    with open(db.get_served_file("conf.json"), "w") as f:
        json.dump(newdata, f)
    db.update_stored_version("config", image.get_config_version(newdata))
    return 'ok', 200

@app.route("/a/stm.bin")
@auth.priv_required(auth.Priv.READ)
def get_stm():
    return send_file(db.get_served_file("stm.bin"), mimetype="application/octet-stream")

@app.route("/a/esp.bin")
@auth.priv_required(auth.Priv.READ)
def get_esp():
    return send_file(db.get_served_file("esp.bin"), mimetype="application/octet-stream")

@app.route("/a/versions")
def get_vers():
    global metric_table, metric_last_sent

    if "X-MSign-Metrics" in request.headers:
        datums = request.headers["X-MSign-Metrics"].split(";")
        metric_last_sent = time.monotonic()
        metric_table_new = {} 
        for v in datums:
            k, v = v.strip().split(" ")
            metric_table_new[k] = v
        metric_table = metric_table_new

    return db.get_served_versions()

@app.route("/a/newui", methods=["POST"])
def update_ui():
    db.update_webui_archive(request.files[next(iter(request.files.keys()))])
    return 'ok', 200

@app.route("/a/updatefirm", methods=["POST"])
def update_firm():
    db.update_firm(request.files["stm"], request.files["esp"])
    return 'ok', 200

@app.route("/a/challenge")
def get_auth_challenge():
    return Response(response=auth.generate_challenge(), status=200, content_type="text/plain")

@app.cli.command("create-user")
@click.argument("username", type=str)
@click.password_option()
@click.option("--priv", type=int, multiple=True, default=(0, 1, 2))
def new_user(username: str, password: str, priv):
    new_user = auth.make_user(username, password, priv)
    click.echo(f"Created new user {new_user['username']} with TOTP {new_user['twofa']}")
    scancode = f"otpauth://totp/msign?secret={new_user['twofa']}"
    qcode = qrcode.QRCode()
    qcode.add_data(scancode)
    qcode.print_ascii(invert=True)

@app.cli.command("create-bearer")
@click.option("--priv", type=int, multiple=True, default=(1,))
def new_user(priv):
    print("Created new token:", auth.make_bearer(priv))

@app.cli.command("test-esp-auth")
def test_esp_auth():
    challenge = auth.generate_challenge()
    blob = auth.compute_shared_blob(challenge)
    print(challenge + ":" + blob)

app.secret_key = db.server_secret
auth.init_app(app)
