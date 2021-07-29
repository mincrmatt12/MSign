# cfgserver

Implements a Vaguely Secure™ protocol for sending configuration over the network.

It exposes a set of HTTPS endpoints which implement a simple authentication scheme and update delivery mechanism.

## Authentication

To authenticate, the ESP must form a valid bearer token. The first step to generating a token is to obtain a _challenge_ from the server, by
performing `GET /challenge`. This will return a blob of text, which is opaque to the ESP (but on the server is a signed message containing
some random nonce and a timestamp). The ESP then signs this message with a shared HMAC key and appends the result (in hex) to the blob. This final string
is used as a bearer token.

For example:

```
GET /challenge HTTP/1.1
...

fahdvu234hauc.fjah3
```

```
GET /whatever HTTP/1.1
Authorization: Bearer fahdvu234hauc.fjah3;22be1f7c
```

Separately, the server stores an administrator user/password/2fa token which can be sent to `POST /login` as

```
{
   "user": "username",
   "password": "password",
   "token": "232148"
}
```

which will set a session cookie that will substitute for valid authentication (this is so that the normal webui can be hosted at the root of the server)

Thirdly, the server can be asked to generate static bearer tokens for use in CI.

These all have different privilege levels:

- The ESP is allowed to download but not change config
- Static tokens are allowed to change but not read config
- The administrator is allowed to do both.

## Update checking

There are two things `cfgserver` can send to the ESP, config updates or system updates. Both of these have an associated "version" identifier. For configuration, the
version is an arbitrary integer. Although it is an integer, a "newer" version does not need to have a larger version, rather the system only checks for a different version.

The version for system upgrades is the version ESP-IDF places into the firmware, which is obtained with `git describe --tags --always --dirty`.

The current version being served can be read from `/version` without authentication:

```
GET /version HTTP/1.1
...

HTTP/1.1 200 OK

{"firm": "v3.1-579-gffe802", "cfg": 4152}
```

If a value is not present, the corresponding file has not been uploaded.

## Downloading updates

The current config can be downloaded from `/conf.json` with valid authentication.

The current firmware is downloadable from `/stm.bin` and `/esp.bin`, again with valid authentication. 

## Uploading updates

Config can be pushed by `POST`-ing to `/conf.json` and firmware can be sent by `POST`-ing to `/updatefirm` with form-encoded files `stm.bin` and `esp.bin`.
