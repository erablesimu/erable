#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Erable
# SPDX-License-Identifier: GPL-3.0-or-later
"""Rewrites the notes of the repository's published releases without Erable's former name, as
PATCHNOTES.md now reads. Env: GITHUB_TOKEN, GITHUB_REPOSITORY. Prints what it changed."""
import json
import os
import urllib.request

REPLACEMENTS = [
    ("**Banst Family devient Erable (suite)**", "**Nouveau nom : Erable (suite)**"),
    ("**Banst Family devient Erable**", "**Nouveau nom : Erable**"),
    ("Recréez ensuite vos raccourcis vers erable.exe : banstdog.exe n'existe plus.",
     "Recréez ensuite vos raccourcis vers erable.exe : l'ancien programme n'existe plus."),
    ("L'ancien dossier `%APPDATA%\\banstdog` est déplacé automatiquement au premier lancement :",
     "L'ancien dossier de données est déplacé automatiquement au premier lancement :"),
    ("- Les anciens raccourcis vers banstdog.exe continuent de marcher : banstdog.exe ouvre erable.exe.",
     "- Les anciens raccourcis continuent de marcher : l'ancien programme ouvre erable.exe."),
    ("Banst Family", "Erable"),
    ("banstdog.exe", "l'ancien programme"),
    ("banstdog", "l'ancien dossier"),
]

API = "https://api.github.com/repos/" + os.environ["GITHUB_REPOSITORY"]
HEADERS = {
    "Authorization": "Bearer " + os.environ["GITHUB_TOKEN"],
    "Accept": "application/vnd.github+json",
    "X-GitHub-Api-Version": "2022-11-28",
}


def call(method, url, body=None):
    data = json.dumps(body).encode() if body is not None else None
    request = urllib.request.Request(url, data=data, method=method, headers=HEADERS)
    with urllib.request.urlopen(request) as response:
        return json.load(response)


def main():
    changed = 0
    page = 1
    while True:
        releases = call("GET", f"{API}/releases?per_page=100&page={page}")
        if not releases:
            break
        for release in releases:
            body = release.get("body") or ""
            name = release.get("name") or ""
            new_body, new_name = body, name
            for old, new in REPLACEMENTS:
                new_body = new_body.replace(old, new)
                new_name = new_name.replace(old, new)
            if new_body != body or new_name != name:
                call("PATCH", f"{API}/releases/{release['id']}", {"body": new_body, "name": new_name})
                print(f"{release['tag_name']}: rewritten")
                changed += 1
        page += 1
    print(f"{changed} release(s) rewritten")


if __name__ == "__main__":
    main()
