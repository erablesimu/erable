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

rooms.json lists the rooms signed-in players may join, with their passwords, and optionally the
game a room is kept for and how many players it takes (see rooms.example.json):
    [{"name": "Salon 1", "port": 24872, "password": "...", "reserved": "Mario Kart World",
      "title_ids": ["0400C3F00006E000"], "keywords": ["Mario Kart World"], "max_players": 8}, ...]

With --room-binary (the eden-room program), this server also runs the rooms: those of rooms.json,
restarted if they stop, and more on demand once every room of a game is full, on the ports of
--dynamic-ports, up to --max-rooms in all; those close after 10 minutes empty.
"""

import argparse
import hashlib
import hmac
import json
import re
import os
import secrets
import sqlite3
import ssl
import subprocess
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
# Messages between friends: kept 15 days, then deleted
MESSAGE_DAYS = 15
MAX_MESSAGE_LENGTH = 500
MAX_MESSAGES_PER_MINUTE = 20
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
CREATE TABLE IF NOT EXISTS messages (
    id INTEGER PRIMARY KEY,
    from_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    to_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    sent_at INTEGER NOT NULL,
    body TEXT NOT NULL,
    read INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS messages_pair ON messages(from_id, to_id, id);
CREATE INDEX IF NOT EXISTS messages_unread ON messages(to_id, read);
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
                  "has_access": self.has_access(account_id),
                  # A nickname from before the filter, or that a newer list catches, is changed
                  # before playing online again
                  "nickname_must_change": is_offensive_nickname(row[0])}
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


# Insults and swear words refused in nicknames: the Latin-script part of the list Erable masks in
# the room chat (src/common/profanity_filter.cpp), with the same folding. Kinds: W the whole word
# (a trailing "s" allowed), P its start, I anywhere; strict ones are compared as written.
PROFANITY = [
    ('fuck', "I", True), ('fuck', "W", False), ('fucking', "W", False), ('fucker', "P", False),
    ('fucked', "W", False), ('shit', "P", True), ('bullshit', "P", True),
    ('horseshit', "W", True), ('batshit', "W", True), ('dipshit', "W", True),
    ('bitch', "P", False), ('bastard', "P", False), ('cunt', "W", True), ('dick', "W", True),
    ('dickhead', "P", False), ('cock', "W", True), ('cocksuck', "P", False), ('pussy', "P", True),
    ('whore', "P", False), ('asshole', "P", False), ('arsehole', "P", False),
    ('wanker', "P", True), ('twat', "P", True), ('prick', "W", True), ('bollock', "P", False),
    ('nigg', "P", True), ('faggot', "P", True), ('tranny', "W", True), ('kike', "W", True),
    ('spic', "W", False), ('chink', "W", False), ('retarded', "W", False), ('merde', "W", True),
    ('merdeux', "P", False), ('merdique', "P", False), ('putain', "P", False),
    ('pute', "W", True), ('salope', "P", False), ('salopard', "P", False), ('salaud', "P", False),
    ('connard', "P", False), ('connasse', "P", False), ('conne', "W", True), ('pédé', "W", True),
    ('tarlouze', "P", False), ('tapette', "W", True), ('encule', "P", False),
    ('enculer', "P", False), ('batard', "P", False), ('fdp', "W", False), ('ntm', "W", False),
    ('nique', "W", False), ('niquer', "P", False), ('niktamer', "P", False),
    ('couille', "P", False), ('couillon', "P", False), ('chier', "W", False),
    ('petasse', "P", False), ('pouffiasse', "P", False), ('grognasse', "P", False),
    ('enfoire', "P", False), ('bougnoule', "P", False), ('nègre', "W", True),
    ('abruti', "P", False), ('cretin', "P", False), ('trouduc', "W", False), ('puta', "W", True),
    ('puto', "W", True), ('mierda', "P", False), ('joder', "P", True), ('jodido', "P", False),
    ('coño', "W", True), ('cabron', "P", False), ('gilipollas', "P", False),
    ('gilipoyas', "P", False), ('pendejo', "P", False), ('maricon', "P", False),
    ('polla', "W", True), ('culero', "W", False), ('chingar', "P", False),
    ('chingada', "P", False), ('verga', "W", True), ('carajo', "W", False),
    ('cojones', "W", False), ('zorra', "W", True), ('cazzo', "W", True), ('cazzi', "W", True),
    ('cazzata', "P", True), ('cazzone', "P", True), ('merda', "W", False),
    ('stronzo', "P", False), ('stronza', "P", False), ('vaffanculo', "P", False),
    ('fanculo', "P", False), ('puttana', "P", False), ('coglione', "P", False),
    ('minchia', "P", False), ('frocio', "P", False), ('froci', "W", False),
    ('bastardo', "P", False), ('scheiße', "P", True), ('scheisse', "P", True),
    ('scheiss', "W", True), ('arschloch', "P", False), ('arsch', "W", False),
    ('fotze', "P", False), ('wichser', "P", False), ('wixer', "W", False),
    ('hurensohn', "P", False), ('hure', "W", True), ('ficken', "P", True),
    ('schlampe', "P", False), ('missgeburt', "P", False), ('spast', "W", False),
    ('schwuchtel', "P", False), ('kanake', "W", False), ('porra', "W", True),
    ('caralho', "P", False), ('foda', "W", True), ('fodase', "W", False), ('fodido', "P", False),
    ('cuzão', "W", True), ('viado', "W", False), ('buceta', "P", False),
    ('arrombado', "P", False), ('kut', "W", True), ('lul', "W", True), ('klootzak', "P", False),
    ('godverdomme', "P", False), ('hoer', "W", False), ('neuken', "P", False),
    ('mongool', "W", True), ('flikker', "P", True), ('tering', "P", False), ('tyfus', "W", True),
    ('kurwa', "P", False), ('chuj', "P", False), ('pierdol', "P", False),
    ('spierdal', "P", False), ('wypierdal', "P", False), ('jebac', "P", False),
    ('jebany', "P", False), ('zajebac', "P", False), ('pizda', "P", False), ('pizdz', "P", False),
    ('skurwysyn', "P", False), ('cipa', "W", True), ('dupek', "W", False), ('cwel', "W", False),
    ('kurva', "P", False), ('kokot', "P", False), ('hovno', "W", False), ('zmrd', "W", False),
    ('čurák', "W", False), ('prdel', "W", False), ('geci', "W", True), ('picsa', "P", False),
    ('fasz', "W", True), ('faszt', "W", False), ('faszom', "W", False), ('fasza', "W", False),
    ('bazmeg', "P", False), ('baszd', "P", False), ('buzi', "W", True), ('köcsög', "P", False),
    ('muie', "W", False), ('futu', "W", False), ('futui', "W", False), ('căcat', "W", True),
    ('poponar', "W", False), ('fitta', "W", True), ('kuk', "W", True), ('jävla', "W", False),
    ('jævla', "W", False), ('knulla', "P", False), ('faen', "W", False), ('pikk', "W", True),
    ('kuksuger', "P", False), ('fisse', "W", True), ('luder', "W", False), ('vittu', "P", True),
    ('vitun', "W", True), ('vitut', "W", True), ('vituttaa', "P", True), ('perkele', "W", True),
    ('paska', "W", True), ('kyrpä', "W", False), ('huora', "W", False), ('jebem', "P", False),
    ('jebote', "P", False), ('jebi', "W", False), ('kurac', "W", False), ('kurca', "W", True),
    ('kurcu', "W", True), ('govno', "P", False), ('orospu', "P", False), ('siktir', "P", False),
    ('yarrak', "P", True), ('malakas', "W", False), ('poustis', "W", False),
    ('poutana', "W", False), ('blyat', "P", False), ('blyad', "P", False), ('suka', "W", True),
    ('cyka', "W", True), ('pizdec', "P", False), ('nahui', "W", True), ('nahuy', "W", True),
    ('pidor', "P", False), ('mudak', "P", False),
]
PROFANITY_ALLOWED = {
    'assassin', 'assassins', 'assassinat', 'bassin', 'classe', 'scunthorpe', 'shitake',
    'shiitake', 'cocktail', 'cocktails', 'cockpit', 'peacock', 'hancock', 'dickens', 'pussycat',
    'snigger', 'sniggered', 'salopette', 'salopettes', 'kurvy', 'prickly', 'dickson', 'putative',
    'computer',
}
LEET = {"0": "o", "1": "i", "!": "i", "|": "i", "3": "e", "4": "a", "@": "a", "5": "s", "$": "s",
        "7": "t", "8": "b"}
SPECIAL_LETTERS = {"ß": "ss", "æ": "ae", "œ": "oe", "ø": "o", "ł": "l", "đ": "d", "þ": "th"}


def fold_letters(text, strict):
    """Lower case; for the loose form, accents gone, the spellings that sound the same written
    one way, then each run of a letter as one"""
    text = text.lower()
    if strict:
        return text
    letters = "".join(
        c for c in unicodedata.normalize("NFKD", "".join(SPECIAL_LETTERS.get(c, c) for c in text))
        if not unicodedata.combining(c))
    letters = letters.replace("ph", "f").replace("ck", "k").replace("y", "i").replace("w", "v")
    return re.sub(r"(.)\1+", r"\1", letters)


FOLDED_PROFANITY = [(fold_letters(word, strict), kind, strict) for word, kind, strict in PROFANITY]
FOLDED_ALLOWED = {fold_letters(word, False) for word in PROFANITY_ALLOWED}


def profanity_match(token):
    loose = fold_letters(token, False)
    if loose in FOLDED_ALLOWED:
        return False
    for word, kind, strict in FOLDED_PROFANITY:
        text = token if strict else loose
        if kind == "W" and text in (word, word + "s"):
            return True
        if kind == "P" and text.startswith(word):
            return True
        if kind == "I" and word in text:
            return True
    return False


def is_offensive_nickname(nickname):
    """Whether a nickname holds an insult or swear word, also in leet, dotted out or run into
    another word ("Big_Connard", "s4l0pe", "c.o.n.n.a.r.d")"""
    # The words of the nickname, also those run together in camel case ("BigConnard")
    spaced = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", " ", nickname)
    letters = "".join(LEET.get(c, c) for c in spaced.lower())
    tokens = [token for token in re.split(r"[^a-zß-ÿĀ-ɏ]+", letters) if token]
    joined = "".join(tokens)
    # The x's framing a gamer tag ("xXkurwaXx")
    tokens.append(joined.strip("x"))
    if any(profanity_match(token) for token in tokens + [joined]):
        return True
    # Run into another word ("BigConnard"): the long listed words anywhere in it, as the shorter
    # ones hide in ordinary words ("conne" in "connection", "tering" in "entering")
    loose_joined = fold_letters(joined, False)
    if loose_joined in FOLDED_ALLOWED:
        return False
    return any(len(word) >= 7 and not strict and word in loose_joined
               for word, _, strict in FOLDED_PROFANITY)


def register(store, body, client):
    nickname = field(body, "nickname", 20).strip()
    email = normalize_email(field(body, "email"))
    password = field(body, "password", 128)
    question = field(body, "question", 64)
    answer = normalize_answer(field(body, "answer"))
    code = field(body, "code", 12)
    if not NICKNAME_RE.fullmatch(nickname):
        raise ApiError(HTTPStatus.BAD_REQUEST, "bad_nickname")
    if is_offensive_nickname(nickname):
        raise ApiError(HTTPStatus.BAD_REQUEST, "offensive_nickname")
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
    require_proper_nickname(store, account_id)
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


def require_proper_nickname(store, account_id):
    """Refuses the online play to an account whose nickname holds an insult, until changed"""
    nickname = store.db.execute("SELECT nickname FROM accounts WHERE id = ?",
                                (account_id,)).fetchone()[0]
    if is_offensive_nickname(nickname):
        raise ApiError(HTTPStatus.FORBIDDEN, "nickname_must_change")


def check_nickname(store, nickname, account_id=None):
    if not NICKNAME_RE.fullmatch(nickname):
        raise ApiError(HTTPStatus.BAD_REQUEST, "bad_nickname")
    if is_offensive_nickname(nickname):
        raise ApiError(HTTPStatus.BAD_REQUEST, "offensive_nickname")
    row = store.db.execute("SELECT id FROM accounts WHERE nickname = ?", (nickname,)).fetchone()
    if row and row[0] != account_id:
        raise ApiError(HTTPStatus.CONFLICT, "nickname_taken")


def change_nickname(store, account_id, body, client):
    """A new nickname: the one an offensive nickname must give way to, or any other"""
    nickname = field(body, "nickname", 20).strip()
    check_nickname(store, nickname, account_id)
    store.check_throttle("nickname:" + str(account_id))
    store.note_failure("nickname:" + str(account_id))
    store.db.execute("UPDATE accounts SET nickname = ? WHERE id = ?", (nickname, account_id))
    store.db.commit()
    return HTTPStatus.OK, store.profile(account_id)


def are_friends(db, a, b):
    return db.execute("SELECT 1 FROM friends WHERE account_id = ? AND friend_id = ?",
                      (a, b)).fetchone() is not None


def purge_messages(db):
    db.execute("DELETE FROM messages WHERE sent_at < ?",
               (int(time.time()) - MESSAGE_DAYS * 86400,))


def send_message(store, account_id, body, client):
    """A message to a friend, kept MESSAGE_DAYS days"""
    require_proper_nickname(store, account_id)
    target = account_by_code(store, body)
    db = store.db
    if not are_friends(db, account_id, target):
        raise ApiError(HTTPStatus.FORBIDDEN, "not_friends")
    text = field(body, "text", MAX_MESSAGE_LENGTH).strip()
    if not text:
        raise ApiError(HTTPStatus.BAD_REQUEST, "bad_text")
    now = int(time.time())
    recent = db.execute("SELECT COUNT(*) FROM messages WHERE from_id = ? AND sent_at > ?",
                        (account_id, now - 60)).fetchone()[0]
    if recent >= MAX_MESSAGES_PER_MINUTE:
        raise ApiError(HTTPStatus.TOO_MANY_REQUESTS, "too_many_messages")
    cursor = db.execute("INSERT INTO messages (from_id, to_id, sent_at, body) VALUES (?, ?, ?, ?)",
                        (account_id, target, now, text))
    db.commit()
    return HTTPStatus.OK, {"id": cursor.lastrowid, "sent_at": now}


def read_messages(store, account_id, body, client):
    """The messages with a friend of the last MESSAGE_DAYS days after the id `after`, oldest
    first; those received are marked read"""
    target = account_by_code(store, body)
    db = store.db
    if not are_friends(db, account_id, target):
        raise ApiError(HTTPStatus.FORBIDDEN, "not_friends")
    after = body.get("after") if isinstance(body.get("after"), int) else 0
    purge_messages(db)
    rows = db.execute(
        "SELECT id, from_id, sent_at, body FROM messages WHERE id > ? AND "
        "((from_id = ? AND to_id = ?) OR (from_id = ? AND to_id = ?)) ORDER BY id LIMIT 200",
        (after, account_id, target, target, account_id)).fetchall()
    db.execute("UPDATE messages SET read = 1 WHERE to_id = ? AND from_id = ? AND read = 0",
               (account_id, target))
    db.commit()
    return HTTPStatus.OK, {"messages": [{"id": row[0], "from_me": row[1] == account_id,
                                         "sent_at": row[2], "text": row[3]} for row in rows]}


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
    unread = dict(db.execute(
        "SELECT a.friend_code, COUNT(*) FROM messages m JOIN accounts a ON a.id = m.from_id "
        "WHERE m.to_id = ? AND m.read = 0 AND m.sent_at >= ? GROUP BY a.friend_code",
        (account_id, now - MESSAGE_DAYS * 86400)).fetchall())
    friends = []
    for nickname, code, seen, friend_game, friend_room in db.execute(
            "SELECT a.nickname, a.friend_code, p.seen_at, p.game, p.room_port FROM friends f "
            "JOIN accounts a ON a.id = f.friend_id LEFT JOIN presence p ON p.account_id = a.id "
            "WHERE f.account_id = ? ORDER BY a.nickname COLLATE NOCASE", (account_id,)):
        online = seen is not None and now - seen <= ONLINE_FOR
        friends.append({"nickname": nickname, "friend_code": code, "online": online,
                        "game": friend_game if online else "",
                        "room_port": friend_room if online else 0,
                        "unread": unread.get(code, 0)})
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
             "password": str(room.get("password", "")),
             # The game the room is kept for ("" for any), how Erable recognises it (program ids,
             # words of its title in any language), and the players it takes
             "reserved": str(room.get("reserved", "")),
             "title_ids": [str(i).upper() for i in room.get("title_ids", [])],
             "keywords": [str(k) for k in room.get("keywords", [])],
             "max_players": max(2, min(16, int(room.get("max_players", 8)))),
             "dynamic": False} for room in rooms]


DYNAMIC_ROOM_IDLE = 600  # An opened room closes after this long empty, in seconds


def players_in_rooms(store, since):
    return dict(store.db.execute(
        "SELECT room_port, COUNT(*) FROM presence WHERE room_port != 0 AND seen_at >= ? "
        "GROUP BY room_port", (since,)).fetchall())


class RoomManager:
    """The rooms: those of rooms.json, and the ones opened on demand once every room of a game is
    full. With an eden-room program it runs them, one process a room: an idle room costs a few
    MiB and no CPU, so the limit that matters is the number of rooms and the bandwidth."""

    def __init__(self, rooms, binary="", dynamic_ports=(), max_rooms=64, log_dir=""):
        self.static = rooms
        self.dynamic = {}  # port -> room
        self.binary = binary
        self.dynamic_ports = list(dynamic_ports)
        self.max_rooms = max_rooms
        self.log_dir = log_dir
        self.processes = {}  # port -> Popen
        self.empty_since = {}  # port -> time an opened room was first seen empty

    def all_rooms(self):
        return self.static + [self.dynamic[port] for port in sorted(self.dynamic)]

    def spawn(self, room):
        if not self.binary:
            return
        port = room["port"]
        command = [self.binary, "--room-name", room["name"], "--port", str(port),
                   "--max-members", str(room["max_players"]),
                   "--preferred-game", room["reserved"] or "Any game",
                   "--preferred-game-id", room["title_ids"][0] if room["title_ids"] else "0"]
        if room["password"]:
            command += ["--password", room["password"]]
        if self.log_dir:
            command += ["--ban-list-file", os.path.join(self.log_dir, f"bans-{port}.txt"),
                        "--log-file", os.path.join(self.log_dir, f"room-{port}.log")]
        self.processes[port] = subprocess.Popen(command, stdin=subprocess.DEVNULL,
                                                stdout=subprocess.DEVNULL,
                                                stderr=subprocess.DEVNULL)

    def start(self):
        for room in self.static:
            self.spawn(room)

    def open(self, store, account_id, reserved):
        """Opens a room for `reserved` ("" for any game) if every room for it is full"""
        if not self.binary or not self.dynamic_ports:
            raise ApiError(HTTPStatus.SERVICE_UNAVAILABLE, "no_room_opening")
        template = next((room for room in self.static if room["reserved"] == reserved), None)
        if template is None:
            raise ApiError(HTTPStatus.BAD_REQUEST, "unknown_room_game")
        same = [room for room in rooms_with_players(store, account_id, self.all_rooms())
                if room["reserved"] == reserved]
        if any(room["players"] < room["max_players"] for room in same):
            raise ApiError(HTTPStatus.CONFLICT, "rooms_available")
        if len(self.static) + len(self.dynamic) >= self.max_rooms:
            raise ApiError(HTTPStatus.SERVICE_UNAVAILABLE, "no_room_left")
        port = next((p for p in self.dynamic_ports
                     if p not in self.dynamic and all(r["port"] != p for r in self.static)), None)
        if port is None:
            raise ApiError(HTTPStatus.SERVICE_UNAVAILABLE, "no_room_left")
        store.check_throttle("open_room:" + str(account_id))
        store.note_failure("open_room:" + str(account_id))
        room = dict(template, name=f"Salon {len(self.static) + len(self.dynamic) + 1}",
                    port=port, password=secrets.token_urlsafe(12), dynamic=True)
        self.dynamic[port] = room
        self.empty_since[port] = time.time()
        self.spawn(room)
        return room

    def reap(self, store):
        """Restarts the rooms that stopped, closes the opened ones empty for a while"""
        now = time.time()
        players = players_in_rooms(store, int(now) - ONLINE_FOR)
        for port, process in list(self.processes.items()):
            if process.poll() is not None:
                room = next((r for r in self.all_rooms() if r["port"] == port), None)
                if room is not None:
                    self.spawn(room)
        for port in list(self.dynamic):
            if players.get(port, 0) > 0:
                self.empty_since.pop(port, None)
                continue
            since = self.empty_since.setdefault(port, now)
            if now - since >= DYNAMIC_ROOM_IDLE:
                process = self.processes.pop(port, None)
                if process is not None:
                    process.terminate()
                del self.dynamic[port]
                self.empty_since.pop(port, None)

    def reap_forever(self, store):
        while True:
            time.sleep(60)
            with store.lock:
                self.reap(store)


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
    ("POST", "/v1/account/nickname"): change_nickname,
    ("POST", "/v1/messages/send"): send_message,
    ("POST", "/v1/messages"): read_messages,
}

ROUTES = {
    ("POST", "/v1/register"): register,
    ("POST", "/v1/login"): login,
    ("POST", "/v1/recovery/question"): recovery_question,
    ("POST", "/v1/recovery"): recover,
}


def make_handler(store, rooms):
    manager = rooms if isinstance(rooms, RoomManager) else RoomManager(rooms)
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
                        require_proper_nickname(store, account_id)
                        self.reply(HTTPStatus.OK, {"rooms": rooms_with_players(
                            store, account_id, manager.all_rooms())})
                        return
                    if (method, self.path) == ("POST", "/v1/rooms/open"):
                        account_id, _ = session_account(store, self.headers)
                        if not store.has_access(account_id):
                            raise ApiError(HTTPStatus.PAYMENT_REQUIRED, "no_access")
                        require_proper_nickname(store, account_id)
                        body = self.read_body()
                        reserved = body.get("reserved") if isinstance(body.get("reserved"),
                                                                      str) else ""
                        room = manager.open(store, account_id, reserved)
                        self.reply(HTTPStatus.OK, {"room": dict(room, players=0, game="")})
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
    parser.add_argument("--room-binary", default="",
                        help="The eden-room program: runs the rooms and opens more on demand")
    parser.add_argument("--dynamic-ports", default="24888-24935",
                        help="The ports of the rooms opened on demand (first-last)")
    parser.add_argument("--max-rooms", type=int, default=64, help="Rooms in all, opened included")
    parser.add_argument("--room-log-dir", default="", help="Where the rooms keep logs and bans")
    args = parser.parse_args()

    store = Store(args.db)
    first, _, last = args.dynamic_ports.partition("-")
    manager = RoomManager(load_rooms(args.rooms), args.room_binary,
                          range(int(first), int(last or first) + 1), args.max_rooms,
                          args.room_log_dir)
    manager.start()
    threading.Thread(target=manager.reap_forever, args=(store,), daemon=True).start()
    server = ThreadingHTTPServer((args.host, args.port), make_handler(store, manager))
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(args.cert, args.key)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    print(f"Erable accounts listening on {args.host}:{args.port}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
