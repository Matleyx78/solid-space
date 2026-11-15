# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors
#
# Tests of scripts/avvia.sh, without network: gh is replaced by a fake that
# answers with the issues given by each test. They check the agent of the
# label (agente:claude-2 is Claude), model and effort read from the
# "**Modello e impegno:**" line of the solid-space issues, and the waiting on
# the "**Dipende da:**" and "**Riferimenti:**" lines. DRY_RUN=1 only prints.
#
#     python3 -m unittest discover -s scripts -p '*_test.py'

from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent / "avvia.sh"

FAKE = r'''#!PYTHON
import json, os, sys
issues = json.load(open(os.environ["FAKE_ISSUES"]))
args = sys.argv[1:]
if args[:1] != ["api"]:
    sys.exit(1)
path = args[1]
number = path.rsplit("/", 1)[1]
data = issues.get(number)
if data is None:
    sys.exit(1)
if "--jq" in args:
    print(data["state"])
else:
    print(json.dumps(data))
'''

# The lines of the open issues of the 0.3 plan, as they are written.
PS7 = ("**Dipende da:** #247 (PS3)\n"
       "**Riferimenti:** #252 (RS7a), #253 (RS7b)\n"
       "**Modello e impegno:** medio (Opus 5.5 medio); impegno alto\n")


def issue(labels: list[str], body: str = "", state: str = "open") -> dict:
    return {"state": state, "labels": [{"name": n} for n in labels], "body": body}


@unittest.skipUnless(shutil.which("jq") and shutil.which("bash"), "needs bash and jq")
class Avvia(unittest.TestCase):
    def run_script(self, issues: dict[str, dict], number: str = "250") -> subprocess.CompletedProcess:
        with tempfile.TemporaryDirectory() as tmp:
            bin_dir = Path(tmp) / "bin"
            bin_dir.mkdir()
            fake = bin_dir / "gh"
            fake.write_text(FAKE.replace("PYTHON", sys.executable))
            fake.chmod(0o755)
            for tool in ("jq", "bash", "grep", "sed", "sort", "head", "tail", "tr"):
                if shutil.which(tool):
                    (bin_dir / tool).symlink_to(shutil.which(tool))
            data = Path(tmp) / "issues.json"
            data.write_text(json.dumps(issues))
            env = {"PATH": str(bin_dir), "FAKE_ISSUES": str(data), "HOME": tmp,
                   "GH_TOKEN": "fake", "DRY_RUN": "1",
                   "GITHUB_REPOSITORY": "Matleyx78/solid-space"}
            return subprocess.run(["bash", str(SCRIPT), number], env=env,
                                  capture_output=True, text=True)

    def closed(self, *numbers: str) -> dict[str, dict]:
        return {n: issue([], state="closed") for n in numbers}

    def test_claude_model_and_effort(self):
        issues = self.closed("247", "252", "253")
        issues["250"] = issue(["pacchetto", "agente:claude"], PS7)
        done = self.run_script(issues)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("[modello: opus] [impegno: alto]", done.stdout)

    def test_claude_2_is_claude(self):
        issues = self.closed("247", "252", "253")
        issues["250"] = issue(["agente:claude-2"],
                              PS7.replace("medio (Opus 5.5 medio); impegno alto",
                                          "leggero (Haiku 5.5 alto); impegno molto alto"))
        done = self.run_script(issues)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("[modello: haiku] [impegno: molto alto]", done.stdout)

    def test_defaults_without_line(self):
        issues = {"250": issue(["agente:claude"])}
        done = self.run_script(issues)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("[modello: opus] [impegno: medio]", done.stdout)

    def test_codex(self):
        issues = self.closed("247", "252", "253")
        issues["250"] = issue(["agente:codex"], PS7)
        done = self.run_script(issues)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("would write: @codex", done.stdout)
        self.assertNotIn("[modello:", done.stdout)

    def test_waits_for_open_dependency_once(self):
        issues = self.closed("247", "253")
        issues["252"] = issue([])
        issues["250"] = issue(["agente:claude"], PS7 + "**Dipende da:** #252\n")
        done = self.run_script(issues)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("waits for: #252 (open): not started", done.stdout)
        self.assertNotIn("would write", done.stdout)

    def test_held_or_closed_not_started(self):
        held = self.run_script({"250": issue(["agente:claude", "AA-Presa in carico"])})
        self.assertIn("held by an agent", held.stdout)
        closed = self.run_script({"250": issue(["agente:claude"], state="closed")})
        self.assertIn("is closed: not started", closed.stdout)

    def test_no_agent_label(self):
        done = self.run_script({"250": issue(["pacchetto", "agente:gemini"])})
        self.assertEqual(done.returncode, 1)


if __name__ == "__main__":
    unittest.main()
