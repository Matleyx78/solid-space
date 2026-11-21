# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The Tecnigrafo authors
# Copyright (C) 2026 The solid-space authors
# Adapted from Tecnigrafo commit a02fa2eac43af5a0aaccea28c22b68e9b3fabca3
#
# Tests of scripts/ci-attendi.sh, without network: gh is replaced by a fake
# that answers with fixed JSON and writes down the API paths it was asked for.
# They check how the argument is read: a short number is a pull request (the
# run of its last commit), a long one a run id, anything else a commit.
#
#     python3 -m unittest discover -s scripts -p '*_test.py'

from __future__ import annotations

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent / "ci-attendi.sh"

FAKE = r'''#!/usr/bin/env bash
path=$2
echo "$path" >> "$FAKE_LOG"
case $path in
    */pulls/*) echo '{"head":{"sha":"abc1234"}}' ;;
    *runs\?head_sha=*) echo '{"workflow_runs":[{"id":4279201937,"status":"completed","conclusion":"success"}]}' ;;
    */runs/*) echo '{"id":4279201937,"status":"completed","conclusion":"success"}' ;;
    *) echo '{}' ;;
esac
'''


@unittest.skipUnless(shutil.which("jq") and shutil.which("bash"), "needs bash and jq")
class Argument(unittest.TestCase):
    def run_script(self, arg: str) -> tuple[subprocess.CompletedProcess, list[str]]:
        with tempfile.TemporaryDirectory() as tmp:
            bin_dir = Path(tmp) / "bin"
            bin_dir.mkdir()
            fake = bin_dir / "gh"
            fake.write_text(FAKE)
            fake.chmod(0o755)
            for tool in ("jq", "bash", "rm", "mktemp", "tail", "sed", "grep"):
                if shutil.which(tool):
                    (bin_dir / tool).symlink_to(shutil.which(tool))
            log = Path(tmp) / "log"
            log.touch()
            env = {"PATH": str(bin_dir), "FAKE_LOG": str(log), "HOME": tmp, "TMPDIR": tmp,
                   "SS_ATTESA_PAUSA": "1", "SS_ATTESA_MAX": "5"}
            done = subprocess.run(["bash", str(SCRIPT), arg], env=env, capture_output=True, text=True)
            return done, log.read_text().splitlines()

    def test_pull_request_number(self):
        done, calls = self.run_script("773")
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("pull request #773: commit abc1234", done.stdout)
        self.assertTrue(calls[0].endswith("/pulls/773"))
        self.assertIn("runs?head_sha=abc1234", calls[1])

    def test_run_id(self):
        done, calls = self.run_script("4279201937")
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertFalse(any("/pulls/" in c for c in calls))
        self.assertTrue(calls[0].endswith("/actions/runs/4279201937"))

    def test_commit(self):
        done, calls = self.run_script("abc1234")
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertFalse(any("/pulls/" in c for c in calls))
        self.assertIn("runs?head_sha=abc1234", calls[0])


# A fake for a run that waits for an approval: RUN_JSON is the run, COMMENTS
# the comments already in the pull request; a POST is written down as such.
FAKE_APPROVAL = r'''#!/usr/bin/env bash
path=$2
if [[ ${3:-} == -f ]]; then
    echo "POST $path" >> "$FAKE_LOG"
    echo '{}'
    exit 0
fi
echo "$path" >> "$FAKE_LOG"
case $path in
    */commits/*/pulls) echo '[{"number":915}]' ;;
    */issues/*/comments*) echo "$COMMENTS" ;;
    *runs\?head_sha=*) echo "{\"workflow_runs\":[$RUN_JSON]}" ;;
    */runs/*/jobs*) echo '{"jobs":[]}' ;;
    *) echo '{}' ;;
esac
'''


@unittest.skipUnless(shutil.which("jq") and shutil.which("bash"), "needs bash and jq")
class Approval(unittest.TestCase):
    """A run in action_required (docs/DECISIONI.md, 2026-11-19)."""

    def run_script(self, created_at: str, comments: str = "[]") -> tuple[subprocess.CompletedProcess, list[str]]:
        run = ('{"id":4279201937,"status":"completed","conclusion":"action_required",'
               '"head_sha":"abc1234def567890","created_at":"%s"}' % created_at)
        with tempfile.TemporaryDirectory() as tmp:
            bin_dir = Path(tmp) / "bin"
            bin_dir.mkdir()
            fake = bin_dir / "gh"
            fake.write_text(FAKE_APPROVAL)
            fake.chmod(0o755)
            for tool in ("jq", "bash", "rm", "mktemp", "tail", "sed", "grep", "cat", "sleep"):
                if shutil.which(tool):
                    (bin_dir / tool).symlink_to(shutil.which(tool))
            log = Path(tmp) / "log"
            log.touch()
            env = {"PATH": str(bin_dir), "FAKE_LOG": str(log), "HOME": tmp, "TMPDIR": tmp,
                   "SS_ATTESA_PAUSA": "1", "SS_ATTESA_MAX": "2", "RUN_JSON": run,
                   "COMMENTS": comments}
            done = subprocess.run(["bash", str(SCRIPT), "abc1234def567890"], env=env,
                                  capture_output=True, text=True)
            return done, log.read_text().splitlines()

    def test_old_run_stops_with_six_and_writes_once(self):
        done, calls = self.run_script("2020-01-01T00:00:00Z")
        self.assertEqual(done.returncode, 6, done.stdout + done.stderr)
        self.assertIn("stop without the label decisione", done.stdout)
        self.assertEqual(sum(c.startswith("POST ") for c in calls), 1)
        self.assertIn("POST repos/Matleyx78/solid-space/issues/915/comments", calls)

    def test_recent_run_keeps_waiting(self):
        done, calls = self.run_script("2999-01-01T00:00:00Z")
        self.assertEqual(done.returncode, 3, done.stdout + done.stderr)
        self.assertIn("waiting for an approval", done.stdout)
        self.assertEqual(sum(c.startswith("POST ") for c in calls), 1)

    def test_note_already_written_is_not_repeated(self):
        comments = '[{"body":"CI da approvare: commit abc1234def56. ..."}]'
        done, calls = self.run_script("2020-01-01T00:00:00Z", comments)
        self.assertEqual(done.returncode, 6, done.stdout + done.stderr)
        self.assertFalse(any(c.startswith("POST ") for c in calls))


if __name__ == "__main__":
    unittest.main()
