#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Erable
# SPDX-License-Identifier: GPL-3.0-or-later
"""Erable accounts: sign-up, sign-in and password recovery, over HTTPS with JSON.

Standard library only (sqlite3, hashlib.scrypt, ssl), so it runs on a stock Debian. Passwords,
security answers and security codes are stored as scrypt hashes; sessions as SHA-256 hashes of
random tokens. Password recovery needs the e-mail, the security answer and the security code
together: nothing is sent by e-mail.

    account_server.py --db /var/lib/erable-accounts/accounts.db \
        --cert cert.pem --key key.pem --port 24870 --rooms rooms.json

rooms.json lists the rooms signed-in players may join, with their passwords:
    [{"name": "Salon 1", "port": 24872, "password": "..."}, ...]
"""

import argparse
import hashlib
import hmac
import json
import re
import secrets
import sqlite3
import ssl
import threading
import time
import unicodedata
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

API_VERSION = 1
SESSION_DAYS = 180
MAX_BODY = 16 * 1024
# Failed sign-ins or recoveries allowed per e-mail and per address before a pause
MAX_FAILURES = 5
FAILURE_WINDOW = 15 * 60

# Friends: requests one account may send a day, the most friends an account may have, and how long
# after its last sync an account still counts as online (Erable syncs every 30 s)
MAX_REQUESTS_PER_DAY = 30
MAX_FRIENDS = 200
ONLINE_FOR = 90

# Access to the rooms: free for every account while this is False. A paid access, later, sets it
# to True; accounts then need access = "paid" with access_until in the future.
REQUIRE_PAID_ACCESS = False

SECURITY_QUESTIONS = (
    "first_pet",
    "birth_city",
    "first_school",
    "childhood_friend",
    "favourite_game",
    "mother_maiden_name",
)

NICKNAME_RE = re.compile(r"^[ A-Za-z0-9._-]{4,20}$")
EMAIL_RE = re.compile(r"^[^@\s]{1,64}@[^@\s]{1,253}\.[^@\s]{2,63}$")

SCHEMA = """
CREATE TABLE IF NOT EXISTS accounts (
    id INTEGER PRIMARY KEY,
    email TEXT NOT NULL UNIQUE,
    nickname TEXT NOT NULL UNIQUE COLLATE NOCASE,
    friend_code TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    question TEXT NOT NULL,
    answer_hash TEXT NOT NULL,
    code_hash TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    access TEXT NOT NULL DEFAULT 'free',
    access_until INTEGER
);
CREATE TABLE IF NOT EXISTS sessions (
    token_hash TEXT PRIMARY KEY,
    account_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    created_at INTEGER NOT NULL,
    expires_at INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS friends (
    account_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    friend_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    since INTEGER NOT NULL,
    PRIMARY KEY (account_id, friend_id)
);
CREATE TABLE IF NOT EXISTS friend_requests (
    from_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    to_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    sent_at INTEGER NOT NULL,
    PRIMARY KEY (from_id, to_id)
);
CREATE TABLE IF NOT EXISTS blocks (
    account_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    blocked_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    PRIMARY KEY (account_id, blocked_id)
);
CREATE TABLE IF NOT EXISTS presence (
    account_id INTEGER PRIMARY KEY REFERENCES accounts(id) ON DELETE CASCADE,
    seen_at INTEGER NOT NULL,
    game TEXT NOT NULL DEFAULT '',
    room_port INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS failures (
    key TEXT NOT NULL,
    at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS failures_key ON failures(key, at);
"""


class ApiError(Exception):
    def __init__(self, status, code):
        super().__init__(code)
        self.status = status
        self.code = code


def hash_secret(secret, salt=None):
    """scrypt with n=2^15, r=8, p=1 (~32 MiB, a few tens of ms), stored as salt$hash in hex"""
    salt = salt or secrets.token_bytes(16)
    digest = hashlib.scrypt(secret.encode("utf-8"), salt=salt, n=1 << 15, r=8, p=1,
                            maxmem=64 * 1024 * 1024, dklen=32)
    return salt.hex() + "$" + digest.hex()


def check_secret(secret, stored):
    salt_hex, _, _ = stored.partition("$")
    return hmac.compare_digest(hash_secret(secret, bytes.fromhex(salt_hex)), stored)


def normalize_answer(answer):
    """Case, accents, spaces and punctuation do not count: "Médor " matches "medor" """
    text = unicodedata.normalize("NFKD", answer.casefold())
    text = "".join(c for c in text if not unicodedata.combining(c))
    return " ".join(re.sub(r"[^\w]+", " ", text).split())


def normalize_email(email):
    return email.strip().lower()


def token_hash(token):
    return hashlib.sha256(token.encode("ascii")).hexdigest()


def new_friend_code(db):
    while True:
        digits = "".join(secrets.choice("0123456789") for _ in range(12))
        code = f"ER-{digits[0:4]}-{digits[4:8]}-{digits[8:12]}"
        if db.execute("SELECT 1 FROM accounts WHERE friend_code = ?", (code,)).fetchone() is None:
            return code


class Store:
    def __init__(self, path):
        self.lock = threading.Lock()
        self.db = sqlite3.connect(path, check_same_thread=False)
        self.db.execute("PRAGMA foreign_keys = ON")
        self.db.execute("PRAGMA journal_mode = WAL")
        self.db.executescript(SCHEMA)
        # Databases made before the access columns
        columns = {row[1] for row in self.db.execute("PRAGMA table_info(accounts)")}
        if "access" not in columns:
            self.db.execute("ALTER TABLE accounts ADD COLUMN access TEXT NOT NULL DEFAULT 'free'")
        if "access_until" not in columns:
            self.db.execute("ALTER TABLE accounts ADD COLUMN access_until INTEGER")
        self.db.commit()

    # Failed attempts, by e-mail and by client address
    def check_throttle(self, *keys):
        since = int(time.time()) - FAILURE_WINDOW
        for key in keys:
            count = self.db.execute("SELECT COUNT(*) FROM failures WHERE key = ? AND at > ?",
                                    (key, since)).fetchone()[0]
            if count >= MAX_FAILURES:
                raise ApiError(HTTPStatus.TOO_MANY_REQUESTS, "too_many_attempts")

    def note_failure(self, *keys):
        now = int(time.time())
        self.db.executemany("INSERT INTO failures (key, at) VALUES (?, ?)",
                            [(key, now) for key in keys])
        self.db.execute("DELETE FROM failures WHERE at < ?", (now - FAILURE_WINDOW,))
        self.db.commit()

    def new_session(self, account_id):
        token = secrets.token_urlsafe(32)
        now = int(time.time())
        self.db.execute("INSERT INTO sessions VALUES (?, ?, ?, ?)",
                        (token_hash(token), account_id, now, now + SESSION_DAYS * 86400))
        return token

    def has_access(self, account_id):
        """Whether the account may use the rooms. Free for everyone for now: a paid access
        will set `access` ("paid") and `access_until` (a Unix time) and be checked here."""
        row = self.db.execute("SELECT access, access_until FROM accounts WHERE id = ?",
                              (account_id,)).fetchone()
        if row is None:
            return False
        access, until = row
        if REQUIRE_PAID_ACCESS:
            return access == "paid" and until is not None and until > time.time()
        return True

    def profile(self, account_id, token=None):
        row = self.db.execute("SELECT nickname, email, friend_code, access, access_until "
                              "FROM accounts WHERE id = ?", (account_id,)).fetchone()
        result = {"nickname": row[0], "email": row[1], "friend_code": row[2],
                  "access": row[3], "access_until": row[4],
                  "has_access": self.has_access(account_id)}
        if token:
            result["token"] = token
        return result


def field(body, name, limit=256):
    value = body.get(name)
    if not isinstance(value, str) or not value or len(value) > limit:
        raise ApiError(HTTPStatus.BAD_REQUEST, f"bad_{name}")
    return value


def check_password(password):
    if len(password) < 8 or len(password) > 128:
        raise ApiError(HTTPStatus.BAD_REQUEST, "weak_password")


def check_code(code):
    if not re.fullmatch(r"[0-9]{6,12}", code):
        raise ApiError(HTTPStatus.BAD_REQUEST, "bad_code")


def register(store, body, client):
    nickname = field(body, "nickname", 20).strip()
    email = normalize_email(field(body, "email"))
    password = field(body, "password", 128)
    question = field(body, "question", 64)
    answer = normalize_answer(field(body, "answer"))
    code = field(body, "code", 12)
    if not NICKNAME_RE.fullmatch(nickname):
        raise ApiError(HTTPStatus.BAD_REQUEST, "bad_nickname")
    if not EMAIL_RE.fullmatch(email):
        raise ApiError(HTTPStatus.BAD_REQUEST, "bad_email")
    check_password(password)
    if question not in SECURITY_QUESTIONS:
        raise ApiError(HTTPStatus.BAD_REQUEST, "bad_question")
    if len(answer) < 2:
        raise ApiError(HTTPStatus.BAD_REQUEST, "bad_answer")
    check_code(code)
    store.check_throttle("register:" + client)
    if store.db.execute("SELECT 1 FROM accounts WHERE email = ?", (email,)).fetchone():
        raise ApiError(HTTPStatus.CONFLICT, "email_taken")
    if store.db.execute("SELECT 1 FROM accounts WHERE nickname = ?", (nickname,)).fetchone():
        raise ApiError(HTTPStatus.CONFLICT, "nickname_taken")
    cursor = store.db.execute(
        "INSERT INTO accounts (email, nickname, friend_code, password_hash, question, "
        "answer_hash, code_hash, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        (email, nickname, new_friend_code(store.db), hash_secret(password), question,
         hash_secret(answer), hash_secret(code), int(time.time())))
    # Sign-ups count against the address too, so one machine can't make thousands of accounts
    store.note_failure("register:" + client)
    token = store.new_session(cursor.lastrowid)
    store.db.commit()
    return HTTPStatus.CREATED, store.profile(cursor.lastrowid, token)


def login(store, body, client):
    email = normalize_email(field(body, "email"))
    password = field(body, "password", 128)
    store.check_throttle("email:" + email, "client:" + client)
    row = store.db.execute("SELECT id, password_hash FROM accounts WHERE email = ?",
                           (email,)).fetchone()
    if row is None or not check_secret(password, row[1]):
        store.note_failure("email:" + email, "client:" + client)
        raise ApiError(HTTPStatus.UNAUTHORIZED, "wrong_credentials")
    token = store.new_session(row[0])
    store.db.commit()
    return HTTPStatus.OK, store.profile(row[0], token)


def recovery_question(store, body, client):
    email = normalize_email(field(body, "email"))
    store.check_throttle("client:" + client)
    row = store.db.execute("SELECT question FROM accounts WHERE email = ?", (email,)).fetchone()
    if row is None:
        store.note_failure("client:" + client)
        raise ApiError(HTTPStatus.NOT_FOUND, "unknown_email")
    return HTTPStatus.OK, {"question": row[0]}


def recover(store, body, client):
    email = normalize_email(field(body, "email"))
    answer = normalize_answer(field(body, "answer"))
    code = field(body, "code", 12)
    password = field(body, "new_password", 128)
    check_password(password)
    store.check_throttle("recover:" + email, "client:" + client)
    row = store.db.execute("SELECT id, answer_hash, code_hash FROM accounts WHERE email = ?",
                           (email,)).fetchone()
    # Both are checked, whatever the first gives, so the time taken tells nothing
    answer_ok = row is not None and check_secret(answer, row[1])
    code_ok = row is not None and check_secret(code, row[2])
    if not (answer_ok and code_ok):
        store.note_failure("recover:" + email, "client:" + client)
        raise ApiError(HTTPStatus.UNAUTHORIZED, "wrong_recovery")
    store.db.execute("UPDATE accounts SET password_hash = ? WHERE id = ?",
                     (hash_secret(password), row[0]))
    # A new password signs every other device out
    store.db.execute("DELETE FROM sessions WHERE account_id = ?", (row[0],))
    token = store.new_session(row[0])
    store.db.commit()
    return HTTPStatus.OK, store.profile(row[0], token)


def delete_account(store, account_id, body, client):
    password = field(body, "password", 128)
    store.check_throttle("delete:" + str(account_id), "client:" + client)
    row = store.db.execute("SELECT password_hash FROM accounts WHERE id = ?",
                           (account_id,)).fetchone()
    if row is None or not check_secret(password, row[0]):
        store.note_failure("delete:" + str(account_id), "client:" + client)
        raise ApiError(HTTPStatus.UNAUTHORIZED, "wrong_credentials")
    # The sessions go with it (ON DELETE CASCADE)
    store.db.execute("DELETE FROM accounts WHERE id = ?", (account_id,))
    store.db.commit()
    return HTTPStatus.OK, {}


def account_by_code(store, body):
    code = field(body, "friend_code", 32).strip().upper()
    row = store.db.execute("SELECT id FROM accounts WHERE friend_code = ?", (code,)).fetchone()
    if row is None:
        raise ApiError(HTTPStatus.NOT_FOUND, "unknown_friend_code")
    return row[0]


def make_friends(store, a, b):
    now = int(time.time())
    store.db.executemany("INSERT OR IGNORE INTO friends VALUES (?, ?, ?)",
                         [(a, b, now), (b, a, now)])
    store.db.execute("DELETE FROM friend_requests WHERE (from_id = ? AND to_id = ?) OR "
                     "(from_id = ? AND to_id = ?)", (a, b, b, a))


def friend_request(store, account_id, body, client):
    target = account_by_code(store, body)
    if target == account_id:
        raise ApiError(HTTPStatus.BAD_REQUEST, "own_friend_code")
    db = store.db
    if db.execute("SELECT 1 FROM friends WHERE account_id = ? AND friend_id = ?",
                  (account_id, target)).fetchone():
        raise ApiError(HTTPStatus.CONFLICT, "already_friends")
    # They asked first: this accepts
    if db.execute("SELECT 1 FROM friend_requests WHERE from_id = ? AND to_id = ?",
                  (target, account_id)).fetchone():
        make_friends(store, account_id, target)
        db.commit()
        return HTTPStatus.OK, {"status": "friends"}
    if db.execute("SELECT 1 FROM friend_requests WHERE from_id = ? AND to_id = ?",
                  (account_id, target)).fetchone():
        raise ApiError(HTTPStatus.CONFLICT, "already_requested")
    since = int(time.time()) - 86400
    sent = db.execute("SELECT COUNT(*) FROM friend_requests WHERE from_id = ? AND sent_at > ?",
                      (account_id, since)).fetchone()[0]
    if sent >= MAX_REQUESTS_PER_DAY:
        raise ApiError(HTTPStatus.TOO_MANY_REQUESTS, "too_many_requests")
    friends = db.execute("SELECT COUNT(*) FROM friends WHERE account_id = ?",
                         (account_id,)).fetchone()[0]
    if friends >= MAX_FRIENDS:
        raise ApiError(HTTPStatus.CONFLICT, "too_many_friends")
    # Someone who blocked this account gets nothing, and the sender is not told
    if not db.execute("SELECT 1 FROM blocks WHERE account_id = ? AND blocked_id = ?",
                      (target, account_id)).fetchone():
        db.execute("INSERT INTO friend_requests VALUES (?, ?, ?)",
                   (account_id, target, int(time.time())))
        db.commit()
    return HTTPStatus.OK, {"status": "requested"}


def friend_respond(store, account_id, body, client):
    sender = account_by_code(store, body)
    accept = body.get("accept") is True
    db = store.db
    if not db.execute("SELECT 1 FROM friend_requests WHERE from_id = ? AND to_id = ?",
                      (sender, account_id)).fetchone():
        raise ApiError(HTTPStatus.NOT_FOUND, "no_request")
    if accept:
        make_friends(store, account_id, sender)
    else:
        db.execute("DELETE FROM friend_requests WHERE from_id = ? AND to_id = ?",
                   (sender, account_id))
    db.commit()
    return HTTPStatus.OK, {"status": "friends" if accept else "refused"}


def friend_remove(store, account_id, body, client):
    other = account_by_code(store, body)
    store.db.execute("DELETE FROM friends WHERE (account_id = ? AND friend_id = ?) OR "
                     "(account_id = ? AND friend_id = ?)", (account_id, other, other, account_id))
    # A request sent to them is withdrawn too
    store.db.execute("DELETE FROM friend_requests WHERE from_id = ? AND to_id = ?",
                     (account_id, other))
    store.db.commit()
    return HTTPStatus.OK, {}


def block_player(store, account_id, body, client):
    other = account_by_code(store, body)
    if other == account_id:
        raise ApiError(HTTPStatus.BAD_REQUEST, "own_friend_code")
    db = store.db
    db.execute("DELETE FROM friends WHERE (account_id = ? AND friend_id = ?) OR "
               "(account_id = ? AND friend_id = ?)", (account_id, other, other, account_id))
    db.execute("DELETE FROM friend_requests WHERE (from_id = ? AND to_id = ?) OR "
               "(from_id = ? AND to_id = ?)", (account_id, other, other, account_id))
    db.execute("INSERT OR IGNORE INTO blocks VALUES (?, ?)", (account_id, other))
    db.commit()
    return HTTPStatus.OK, {}


def sync(store, account_id, body, client):
    """Notes that the account is online (and in which game and room), and gives its friends with
    theirs and its friend requests: Erable calls it every 30 s"""
    game = body.get("game") if isinstance(body.get("game"), str) else ""
    room = body.get("room_port") if isinstance(body.get("room_port"), int) else 0
    now = int(time.time())
    db = store.db
    db.execute("INSERT INTO presence (account_id, seen_at, game, room_port) VALUES (?, ?, ?, ?) "
               "ON CONFLICT(account_id) DO UPDATE SET seen_at = excluded.seen_at, "
               "game = excluded.game, room_port = excluded.room_port",
               (account_id, now, game[:96], room if 0 <= room <= 65535 else 0))
    db.commit()
    friends = []
    for nickname, code, seen, friend_game, friend_room in db.execute(
            "SELECT a.nickname, a.friend_code, p.seen_at, p.game, p.room_port FROM friends f "
            "JOIN accounts a ON a.id = f.friend_id LEFT JOIN presence p ON p.account_id = a.id "
            "WHERE f.account_id = ? ORDER BY a.nickname COLLATE NOCASE", (account_id,)):
        online = seen is not None and now - seen <= ONLINE_FOR
        friends.append({"nickname": nickname, "friend_code": code, "online": online,
                        "game": friend_game if online else "",
                        "room_port": friend_room if online else 0})
    incoming = [{"nickname": nickname, "friend_code": code, "sent_at": sent}
                for nickname, code, sent in db.execute(
                    "SELECT a.nickname, a.friend_code, r.sent_at FROM friend_requests r "
                    "JOIN accounts a ON a.id = r.from_id WHERE r.to_id = ? ORDER BY r.sent_at",
                    (account_id,))]
    outgoing = [{"nickname": nickname, "friend_code": code, "sent_at": sent}
                for nickname, code, sent in db.execute(
                    "SELECT a.nickname, a.friend_code, r.sent_at FROM friend_requests r "
                    "JOIN accounts a ON a.id = r.to_id WHERE r.from_id = ? ORDER BY r.sent_at",
                    (account_id,))]
    return HTTPStatus.OK, {"friends": friends, "incoming": incoming, "outgoing": outgoing}


def load_rooms(path):
    if not path:
        return []
    with open(path, encoding="utf-8") as file:
        rooms = json.load(file)
    return [{"name": str(room["name"]), "port": int(room["port"]),
             "password": str(room.get("password", ""))} for room in rooms]


def rooms_with_players(store, account_id, rooms):
    """The rooms, each with the other players online in it and the game they play there: a
    room holds one game at a time, the one its players run, and is free again once empty"""
    now = int(time.time())
    by_port = {}
    for port, game in store.db.execute(
            "SELECT room_port, game FROM presence WHERE account_id != ? AND room_port != 0 "
            "AND seen_at >= ?", (account_id, now - ONLINE_FOR)):
        players, games = by_port.setdefault(port, [0, {}])
        by_port[port][0] = players + 1
        if game:
            games[game] = games.get(game, 0) + 1
    listed = []
    for room in rooms:
        players, games = by_port.get(room["port"], [0, {}])
        game = max(games, key=games.get) if games else ""
        listed.append(dict(room, players=players, game=game))
    return listed


def session_account(store, headers):
    auth = headers.get("Authorization", "")
    if not auth.startswith("Bearer "):
        raise ApiError(HTTPStatus.UNAUTHORIZED, "no_session")
    hashed = token_hash(auth[len("Bearer "):].strip())
    row = store.db.execute("SELECT account_id, expires_at FROM sessions WHERE token_hash = ?",
                           (hashed,)).fetchone()
    if row is None or row[1] < time.time():
        raise ApiError(HTTPStatus.UNAUTHORIZED, "no_session")
    return row[0], hashed


# Routes for a signed-in account: (store, account id, body, client address)
ACCOUNT_ROUTES = {
    ("POST", "/v1/account/delete"): delete_account,
    ("POST", "/v1/friends/request"): friend_request,
    ("POST", "/v1/friends/respond"): friend_respond,
    ("POST", "/v1/friends/remove"): friend_remove,
    ("POST", "/v1/friends/block"): block_player,
    ("POST", "/v1/sync"): sync,
}

ROUTES = {
    ("POST", "/v1/register"): register,
    ("POST", "/v1/login"): login,
    ("POST", "/v1/recovery/question"): recovery_question,
    ("POST", "/v1/recovery"): recover,
}


def make_handler(store, rooms):
    class Handler(BaseHTTPRequestHandler):
        server_version = "ErableAccounts/1"
        sys_version = ""

        def log_message(self, format, *args):
            # No bodies, e-mails or tokens in the log: only the request line and the status
            pass

        def reply(self, status, payload):
            data = json.dumps(payload).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

        def read_body(self):
            length = int(self.headers.get("Content-Length") or 0)
            if length <= 0 or length > MAX_BODY:
                raise ApiError(HTTPStatus.BAD_REQUEST, "bad_body")
            try:
                body = json.loads(self.rfile.read(length))
            except ValueError:
                raise ApiError(HTTPStatus.BAD_REQUEST, "bad_body") from None
            if not isinstance(body, dict):
                raise ApiError(HTTPStatus.BAD_REQUEST, "bad_body")
            return body

        def handle_request(self, method):
            try:
                with store.lock:
                    if (method, self.path) == ("GET", "/v1/status"):
                        self.reply(HTTPStatus.OK, {"api": API_VERSION,
                                                   "questions": list(SECURITY_QUESTIONS)})
                        return
                    if (method, self.path) == ("GET", "/v1/me"):
                        account_id, _ = session_account(store, self.headers)
                        self.reply(HTTPStatus.OK, store.profile(account_id))
                        return
                    if (method, self.path) == ("GET", "/v1/rooms"):
                        account_id, _ = session_account(store, self.headers)
                        if not store.has_access(account_id):
                            raise ApiError(HTTPStatus.PAYMENT_REQUIRED, "no_access")
                        self.reply(HTTPStatus.OK, {"rooms": rooms_with_players(store, account_id,
                                                                              rooms)})
                        return
                    account_route = ACCOUNT_ROUTES.get((method, self.path))
                    if account_route is not None:
                        account_id, _ = session_account(store, self.headers)
                        status, payload = account_route(store, account_id, self.read_body(),
                                                        self.client_address[0])
                        self.reply(status, payload)
                        return
                    if (method, self.path) == ("POST", "/v1/logout"):
                        _, hashed = session_account(store, self.headers)
                        store.db.execute("DELETE FROM sessions WHERE token_hash = ?", (hashed,))
                        store.db.commit()
                        self.reply(HTTPStatus.OK, {})
                        return
                    route = ROUTES.get((method, self.path))
                    if route is None:
                        raise ApiError(HTTPStatus.NOT_FOUND, "not_found")
                    status, payload = route(store, self.read_body(), self.client_address[0])
                self.reply(status, payload)
            except ApiError as error:
                self.reply(error.status, {"error": error.code})
            except Exception:  # noqa: BLE001 - never leak details to the client
                store.db.rollback()
                self.reply(HTTPStatus.INTERNAL_SERVER_ERROR, {"error": "server_error"})

        def do_GET(self):
            self.handle_request("GET")

        def do_POST(self):
            self.handle_request("POST")

    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--db", required=True)
    parser.add_argument("--cert", required=True)
    parser.add_argument("--key", required=True)
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=24870)
    parser.add_argument("--rooms", default="", help="JSON list of the rooms and their passwords")
    args = parser.parse_args()

    store = Store(args.db)
    server = ThreadingHTTPServer((args.host, args.port), make_handler(store, load_rooms(args.rooms)))
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(args.cert, args.key)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    print(f"Erable accounts listening on {args.host}:{args.port}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
