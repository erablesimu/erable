import json
import os
import sqlite3
import tempfile
import unittest

import export_bugs


def issue(number, title, body="", state="open", **extra):
    return {
        "number": number, "title": title, "body": body, "state": state, "labels": [{"name": "bug"}],
        "comments": 2, "created_at": "2026-11-14T10:00:00Z", "updated_at": "2026-11-14T11:00:00Z",
        "closed_at": None, "html_url": f"https://github.com/erablesimu/erable/issues/{number}", **extra,
    }


FORM_FR = """### Jeu

The Legend of Zelda: Ocarina of Time

### Type de problème

Image fausse

### Version d'Erable

build-24

### Ce qui se passe

Tout est noir

### Journal

_Glissez ici `erable_log.txt`._
"""

FORM_DE = """### Spiel

Kein bestimmtes

### Art des Problems

Absturz

### Erable-Version

Nicht angegeben

### Was passiert

Stürzt beim Start ab
"""


class ExportTest(unittest.TestCase):
    def export(self, issues):
        directory = tempfile.mkdtemp()
        path = os.path.join(directory, "bugs.sqlite")
        count = export_bugs.write(path, issues, "erablesimu/erable")
        db = sqlite3.connect(path)
        db.row_factory = sqlite3.Row
        return count, {row["number"]: dict(row) for row in db.execute("SELECT * FROM bugs")}

    def test_form_issue_in_french(self):
        _, rows = self.export([issue(1, "[The Legend of Zelda: Ocarina of Time] [Image fausse] Écran noir", FORM_FR)])
        row = rows[1]
        self.assertEqual(row["summary"], "Écran noir")
        self.assertEqual(row["game"], "The Legend of Zelda: Ocarina of Time")
        self.assertEqual(row["kind"], "graphics")
        self.assertEqual(row["version"], "build-24")
        self.assertEqual(row["details"], "Tout est noir")
        self.assertEqual(json.loads(row["labels"]), ["bug"])

    def test_form_issue_in_german_without_game_or_version(self):
        _, rows = self.export([issue(2, "[Absturz] Absturz beim Start", FORM_DE)])
        row = rows[2]
        self.assertEqual(row["summary"], "Absturz beim Start")
        self.assertIsNone(row["game"])
        self.assertEqual(row["kind"], "crash")
        self.assertIsNone(row["version"])

    def test_first_form_version_and_plain_issues(self):
        _, rows = self.export([
            issue(3, "[Mario Kart World] Son haché", "### Type de problème\n\nSon absent ou haché\n"),
            issue(4, "Crash in the menu", None, state="closed", state_reason="completed",
                  closed_at="2026-11-15T00:00:00Z"),
        ])
        self.assertEqual((rows[3]["game"], rows[3]["kind"], rows[3]["summary"]),
                         ("Mario Kart World", "sound", "Son haché"))
        self.assertEqual((rows[4]["summary"], rows[4]["game"], rows[4]["kind"], rows[4]["state"]),
                         ("Crash in the menu", None, None, "closed"))

    def test_pull_requests_are_left_out(self):
        count, rows = self.export([issue(5, "Fix"), issue(6, "PR", pull_request={"url": "x"})])
        self.assertEqual((count, sorted(rows)), (1, [5]))


if __name__ == "__main__":
    unittest.main()
