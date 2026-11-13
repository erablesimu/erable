#!/usr/bin/env python3
"""Writes the repository's issues to an SQLite file for the site's bug list.

    export_bugs.py OUTPUT.sqlite [--repo owner/name] [--from-json issues.json]

Reads the issues from the GitHub API (GITHUB_TOKEN or GH_TOKEN, GITHUB_REPOSITORY) or, for
tests, from a JSON file holding the API's issue objects. Pull requests are left out. The
title and body that the site's report form writes ("[Game] [Kind] summary", "### Heading"
sections in French, English or German) are split into columns; other issues keep their
title as the summary.
"""

import argparse
import json
import os
import re
import sqlite3
import sys
import urllib.request
from datetime import datetime, timezone

# The report form's kinds (site/i18n.js report.k1..k8) and earlier wordings, by key
KINDS = {
    "crash": ["plantage", "crash", "absturz"],
    "graphics": ["image", "wrong image", "falsches bild"],
    "sound": ["son", "sound", "ton"],
    "performance": ["lenteur", "slowness", "ruckeln"],
    "stuck": ["jeu bloqué", "le jeu bloque", "game stuck", "spiel hängt"],
    "controller": ["manette", "controller"],
    "install": ["installation"],
    "other": ["autre", "other", "sonstiges"],
}

# The body's section headings, by column
SECTIONS = {
    "game": ["jeu", "game", "spiel"],
    "kind": ["type de problème", "kind of problem", "art des problems"],
    "version": ["version d'erable", "erable version", "erable-version"],
    "details": ["ce qui se passe", "what happens", "was passiert"],
}

UNSET = {"aucun en particulier", "none in particular", "kein bestimmtes", "non précisée", "not given",
         "nicht angegeben"}

SCHEMA = """
CREATE TABLE bugs (
    number INTEGER PRIMARY KEY,
    summary TEXT NOT NULL,
    game TEXT,
    kind TEXT,
    version TEXT,
    details TEXT,
    state TEXT NOT NULL,          -- open, closed
    state_reason TEXT,            -- completed, not_planned, reopened...
    labels TEXT NOT NULL,         -- JSON array of label names
    comments INTEGER NOT NULL,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    closed_at TEXT,
    url TEXT NOT NULL
);
CREATE INDEX bugs_state ON bugs(state);
CREATE INDEX bugs_game ON bugs(game);
CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);
"""


def kind_key(text):
    text = (text or "").strip().lower()
    if not text:
        return None
    for key, words in KINDS.items():
        if any(text == word or text.startswith(word) for word in words):
            return key
    return "other"


def sections(body):
    """The body's '### Heading' sections, by lowercased heading."""
    found = {}
    for match in re.finditer(r"^###\s+(.+?)\s*$\n(.*?)(?=^###\s|\Z)", body or "", re.M | re.S):
        found[match.group(1).strip().lower()] = match.group(2).strip()
    return found


def field(found, column):
    for heading in SECTIONS[column]:
        value = found.get(heading)
        if value and value.lower() not in UNSET:
            return value
    return None


def parse(issue):
    title = issue["title"].strip()
    found = sections(issue.get("body"))
    prefixes = []
    while (match := re.match(r"^\[([^\]]+)\]\s*(.*)$", title)) and len(prefixes) < 2:
        prefixes.append(match.group(1).strip())
        title = match.group(2)
    game = field(found, "game")
    kind = kind_key(field(found, "kind"))
    if prefixes:
        # "[Game] [Kind] summary", or "[Game] summary" from the form's first version
        if len(prefixes) == 2:
            game = game or prefixes[0]
            kind = kind or kind_key(prefixes[1])
        elif kind_key(prefixes[0]) != "other":
            kind = kind or kind_key(prefixes[0])
        else:
            game = game or prefixes[0]
    details = field(found, "details")
    if details and len(details) > 600:
        details = details[:597].rstrip() + "…"
    return {
        "number": issue["number"],
        "summary": title or issue["title"],
        "game": game,
        "kind": kind,
        "version": field(found, "version"),
        "details": details,
        "state": issue["state"],
        "state_reason": issue.get("state_reason"),
        "labels": json.dumps([label["name"] for label in issue.get("labels", [])], ensure_ascii=False),
        "comments": issue.get("comments", 0),
        "created_at": issue["created_at"],
        "updated_at": issue["updated_at"],
        "closed_at": issue.get("closed_at"),
        "url": issue["html_url"],
    }


def fetch(repo, token):
    issues = []
    url = f"https://api.github.com/repos/{repo}/issues?state=all&per_page=100"
    while url:
        request = urllib.request.Request(url, headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "erable-site",
            **({"Authorization": f"Bearer {token}"} if token else {}),
        })
        with urllib.request.urlopen(request, timeout=30) as response:
            issues += json.load(response)
            links = response.headers.get("Link", "")
        match = re.search(r'<([^>]+)>;\s*rel="next"', links)
        url = match.group(1) if match else None
    return issues


def write(path, issues, repo):
    if os.path.exists(path):
        os.remove(path)
    rows = [parse(issue) for issue in issues if "pull_request" not in issue]
    with sqlite3.connect(path) as db:
        db.executescript(SCHEMA)
        db.executemany(
            "INSERT INTO bugs VALUES (:number, :summary, :game, :kind, :version, :details, :state, "
            ":state_reason, :labels, :comments, :created_at, :updated_at, :closed_at, :url)", rows)
        db.executemany("INSERT INTO meta VALUES (?, ?)", [
            ("repo", repo),
            ("generated_at", datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")),
        ])
    db.close()
    return len(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("output")
    parser.add_argument("--repo", default=os.environ.get("GITHUB_REPOSITORY", "erablesimu/erable"))
    parser.add_argument("--from-json", help="read the issues from this file instead of GitHub")
    args = parser.parse_args()
    if args.from_json:
        with open(args.from_json, encoding="utf-8") as file:
            issues = json.load(file)
    else:
        issues = fetch(args.repo, os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN"))
    count = write(args.output, issues, args.repo)
    print(f"{count} issues written to {args.output}")


if __name__ == "__main__":
    sys.exit(main())
