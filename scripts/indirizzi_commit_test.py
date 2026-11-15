# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors
#
# Taken from scripts/indirizzi_commit_test.py of Matleyx78/tecnigrafo (GPL-3.0-or-later),
# same maintainer, at the maintainer's request.
#
# Tests of scripts/indirizzi_commit.py: the script runs on a temporary
# repository with commits written here.
#
#     python3 -m unittest discover -s scripts -p '*_test.py'

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPT = HERE / "indirizzi_commit.py"
sys.path.insert(0, str(HERE))

import indirizzi_commit  # noqa: E402

OWNER = "18114626+Matleyx78@users.noreply.github.com"
CLAUDE = "Co-authored-by: Claude <noreply@anthropic.com>"
PERSONAL = "someone@example.com"


class Repo:
    def __init__(self, path: Path):
        self.path = path
        self.git("init", "-q", "-b", "main")
        self.base = self.commit("base", OWNER)

    def git(self, *args: str, env: dict | None = None) -> str:
        full = {k: v for k, v in os.environ.items()
                if not k.startswith("GIT_")}
        full.update({"GIT_CONFIG_GLOBAL": os.devnull,
                     "GIT_CONFIG_NOSYSTEM": "1"})
        full.update(env or {})
        return subprocess.run(["git", *args], cwd=self.path, env=full,
                              check=True, capture_output=True,
                              text=True).stdout.strip()

    def commit(self, message: str, author: str,
               committer: str | None = None) -> str:
        env = {"GIT_AUTHOR_NAME": "A", "GIT_AUTHOR_EMAIL": author,
               "GIT_COMMITTER_NAME": "C",
               "GIT_COMMITTER_EMAIL": committer or author}
        self.git("commit", "-q", "--allow-empty", "-m", message, env=env)
        return self.git("rev-parse", "--short", "HEAD")

    def run(self) -> subprocess.CompletedProcess:
        return subprocess.run(
            [sys.executable, str(SCRIPT), self.base, "HEAD"], cwd=self.path,
            capture_output=True, text=True, encoding="utf-8")


def errors(out: str) -> list[str]:
    return [line for line in out.splitlines() if line.startswith("::error")]


class Script(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.repo = Repo(Path(tmp.name))

    def test_personal_author_is_reported_alone(self):
        self.repo.commit("feat: owner", OWNER)
        self.repo.commit(f"feat: agent\n\nBody.\n\n{CLAUDE}", OWNER)
        bad = self.repo.commit("feat: personal", PERSONAL, OWNER)
        res = self.repo.run()
        self.assertEqual(res.returncode, 1)
        lines = errors(res.stdout)
        self.assertEqual(len(lines), 1)
        self.assertIn(bad, lines[0])
        self.assertIn("author", lines[0])
        self.assertIn("--reset-author", res.stdout)
        self.assertIn("--force-with-lease", res.stdout)

    def test_personal_committer_and_co_author_are_reported(self):
        by_committer = self.repo.commit("fix: committer", OWNER, PERSONAL)
        by_trailer = self.repo.commit(
            f"fix: trailer\n\n{CLAUDE}\nCo-authored-by: Someone <{PERSONAL}>",
            OWNER)
        res = self.repo.run()
        self.assertEqual(res.returncode, 1)
        lines = errors(res.stdout)
        self.assertEqual(len(lines), 2)
        self.assertTrue(any(by_committer in x and "committer" in x
                            for x in lines))
        self.assertTrue(any(by_trailer in x and "Co-authored-by" in x
                            for x in lines))

    def test_allowed_addresses_pass(self):
        self.repo.commit(f"feat: a\n\n{CLAUDE}", OWNER, "noreply@github.com")
        self.repo.commit("feat: b\n\nCo-authored-by: Codex <codex@openai.com>",
                         "41898282+github-actions[bot]@users.noreply.github.com")
        res = self.repo.run()
        self.assertEqual(res.returncode, 0, res.stdout + res.stderr)
        self.assertEqual(errors(res.stdout), [])

    def test_address_is_masked_in_the_output(self):
        self.repo.commit("feat: personal", PERSONAL)
        res = self.repo.run()
        self.assertEqual(res.returncode, 1)
        self.assertNotIn(PERSONAL, res.stdout + res.stderr)
        self.assertIn("s…@example.com", errors(res.stdout)[0])


class Functions(unittest.TestCase):
    def test_allowed(self):
        self.assertTrue(indirizzi_commit.allowed(OWNER))
        self.assertTrue(indirizzi_commit.allowed("NoReply@Anthropic.com"))
        self.assertFalse(indirizzi_commit.allowed(PERSONAL))
        self.assertFalse(indirizzi_commit.allowed("x@noreply.github.com.evil"))

    def test_mask(self):
        self.assertEqual(indirizzi_commit.mask("luca@example.com"),
                         "l…@example.com")
        self.assertEqual(indirizzi_commit.mask("nobody"), "n…")

    def test_co_authors(self):
        self.assertEqual(
            indirizzi_commit.co_authors(f"x\n\n{CLAUDE}\nco-authored-by: a@b.c"),
            ["noreply@anthropic.com", "a@b.c"])


if __name__ == "__main__":
    unittest.main()
