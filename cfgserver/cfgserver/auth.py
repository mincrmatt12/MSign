from flask_login import LoginManager, UserMixin as FlaskUserMixin, AnonymousUserMixin, login_user, login_required, current_user
from flask import g
from werkzeug.exceptions import Unauthorized
from flask.sessions import SecureCookieSessionInterface
from . import db
from tinydb import where
from functools import wraps
import enum
import itsdangerous
import hmac
import hashlib
import binascii
import base64
import mintotp
import secrets

class CustomSessionInterface(SecureCookieSessionInterface):
    def save_session(self, *args, **kwargs):
        if g.get('login_via_header'):
            return
        return super(CustomSessionInterface, self).save_session(*args, **kwargs)


login_manager = LoginManager()
esp_signer = itsdangerous.TimestampSigner(secret_key=db.server_secret, salt="espsigner")

def compute_shared_blob(server_blob):
    return binascii.hexlify(
        hmac.digest(db.shared_secret, server_blob.encode('ascii'), "sha256")
    ).decode('ascii')

def compute_passhash(password: str, salt: str):
    """
    Compute the passhash for a plaintext password
    """

    return base64.b64encode(hashlib.pbkdf2_hmac('sha512', password.encode('utf-8'), base64.b64decode(salt.encode('ascii')), 130000)).decode('ascii')

class Priv(enum.Enum):
    READ = 0
    WRITE = 1
    ADMIN = 2

def priv_required(*privs):
    def gen(func):
        @wraps(func)
        @login_required
        def wrapper(*args, **kwargs):
            if not all(current_user.has(x) for x in privs):
                raise Unauthorized()
            else:
                return func(*args, **kwargs)
        return wrapper
    return gen

class User:
    @property
    def privileges(self):
        return []

    def has(self, priv):
        return priv in self.privileges

class DbUser(User, FlaskUserMixin):
    def __init__(self, data):
        self.data = data

    @property
    def privileges(self):
        return [Priv(x) for x in self.data["privs"]]

    def get_id(self):
        return self.data.doc_id

class AnonUser(User, AnonymousUserMixin):
    pass

class EspUser(User, FlaskUserMixin):
    @property
    def privileges(self):
        return [Priv.READ]

    def get_id(self):
        return "esp_user_id"

@login_manager.user_loader
def load_user(user_id):
    return DbUser(db.users.get(doc_id=user_id))

@login_manager.request_loader
def load_user_from_auth(request):
    auth = request.headers.get("Authorization")
    if not auth:
        return None
    g.login_via_header = True
    auth_type, auth_payload = auth.split(" ")
    if auth_type == "Bearer":
        return DbUser(db.bearer_tokens.get(where("token") == auth_payload))
    elif auth_type == "Chip":
        server_blob, client_blob = auth_payload.split(":")

        if not esp_signer.validate(server_blob, max_age=90):
            return None

        if client_blob != compute_shared_blob(server_blob):
            return None

        return EspUser()

def generate_challenge():
    nonce = secrets.token_urlsafe(20)
    return esp_signer.sign(nonce).decode('ascii')

def try_login_user(username, password, twofa):
    relevant_user = db.users.get(where("username") == username)
    if not relevant_user:
        return False
    
    correct_hash = compute_passhash(password, relevant_user["salt"])
    if correct_hash != relevant_user["hash"]:
        return False

    correct_totp = mintotp.totp(relevant_user["twofa"])
    if correct_totp != twofa:
        return False

    login_user(DbUser(relevant_user))
    return True

def make_user(username, password, privilege_nums=(0,1)):
    salt = base64.b64encode(secrets.token_bytes(64)).decode('ascii')
    twofa = base64.b32encode(secrets.token_bytes(20)).decode('ascii')
    model = {
        "username": username,
        "salt": salt,
        "hash": compute_passhash(password, salt),
        "twofa": twofa,
        "privs": privilege_nums
    }
    db.users.insert(model)
    return model

def make_bearer(privilege_nums=(1,)):
    thetoken = secrets.token_urlsafe(128)
    db.bearer_tokens.insert({
        "token": thetoken,
        "privs": privilege_nums
    })
    return thetoken

def init_app(app):
    app.session_interface = CustomSessionInterface()
    login_manager.init_app(app)
