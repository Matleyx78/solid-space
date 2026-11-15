# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The Tecnigrafo authors
# Copyright (C) 2026 The solid-space authors
# Adapted from Tecnigrafo commit a02fa2eac43af5a0aaccea28c22b68e9b3fabca3
#
# Tests of scripts/sentinella.py, without network: the choice of the stuck
# chains (docs/agenti/catena.md, "Sentinella"), with GitHub replaced by
# answers written here.
#
#     python3 -m unittest discover -s scripts -p '*_test.py'

from __future__ import annotations

import datetime as dt
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import sentinella  # noqa: E402

OWNER = "Matleyx78"


def stuck(it, soglia):
    return sentinella.stuck(it, soglia, OWNER)


def find(raw, now, soglia, gh):
    return sentinella.find(raw, now, soglia, gh, OWNER)


def comment(body: str, login: str = OWNER) -> dict:
    return {"login": login, "body": body, "created_at": "2026-11-14T18:30:00Z"}


def item(**kw) -> sentinella.Item:
    base = dict(number=7, is_pr=False, labels={"agente:codex"}, idle=90.0,
                comments=[comment("Presa in carico da Codex")])
    base.update(kw)
    return sentinella.Item(**base)


class Stuck(unittest.TestCase):
    def test_started_issue_without_movement_is_stuck(self):
        text = stuck(item(), 60)
        self.assertIsNotNone(text)
        self.assertIn("Catena ferma", text)
        self.assertIn("90 minuti", text)

    def test_threshold_and_upper_bound(self):
        self.assertIsNone(stuck(item(idle=59.0), 60))
        self.assertIsNotNone(stuck(item(idle=60.0), 60))
        self.assertIsNone(stuck(item(idle=24 * 60 + 1.0), 60))

    def test_not_started_is_left_alone(self):
        self.assertIsNone(stuck(item(comments=[comment("Una nota")]), 60))

    def test_issue_without_agent_label_is_left_alone(self):
        self.assertIsNone(stuck(item(labels={"idea"}), 60))

    def test_stop_labels(self):
        for label in ("pronta", "ferma", "proposta"):
            with self.subTest(label=label):
                self.assertIsNone(stuck(item(labels={"agente:claude", label}), 60))

    def test_issue_handled_by_open_pull_request(self):
        self.assertIsNone(stuck(item(handled_by_pr=True), 60))

    def test_pull_request_with_running_ci(self):
        pr = item(is_pr=True, labels={"AA-Presa in carico"})
        self.assertIsNotNone(stuck(pr, 60))
        pr.ci_running = True
        self.assertIsNone(stuck(pr, 60))

    def test_held_label_is_reported(self):
        text = stuck(item(labels={"agente:claude", "AA-Presa in carico"}), 60)
        self.assertIn("AA-Presa in carico", text)

    def test_decision_waiting_for_the_owner(self):
        it = item(labels={"agente:codex", "decisione"},
                  comments=[comment("Presa in carico da Codex"),
                            comment("Serve una decisione del responsabile: …")])
        self.assertIsNone(stuck(it, 60))

    def test_decision_written_and_not_picked_up(self):
        it = item(labels={"agente:codex", "decisione"},
                  comments=[comment("Presa in carico da Codex"),
                            comment("@codex Decisione del responsabile: A"),
                            comment("You have reached your usage limit", "chatgpt-codex-connector[bot]")])
        text = stuck(it, 60)
        self.assertIsNotNone(text)
        self.assertIn("Decisione non ripresa", text)

    def test_decision_with_another_stop_label(self):
        it = item(labels={"agente:codex", "decisione", "ferma"},
                  comments=[comment("@codex Decisione del responsabile: A")])
        self.assertIsNone(stuck(it, 60))

    def test_contributor_comment_is_not_a_decision(self):
        # A contributor writes a line that looks like a decision: only the
        # owner's comments count, so nothing is "not picked up".
        it = item(labels={"agente:codex", "decisione"},
                  comments=[comment("Presa in carico da Codex"),
                            comment("Serve una decisione del responsabile: …"),
                            comment("@codex Decisione del responsabile: A", "contributore")])
        self.assertIsNone(stuck(it, 60))

    def test_contributor_comment_does_not_hide_the_owner_decision(self):
        it = item(labels={"agente:codex", "decisione"},
                  comments=[comment("Presa in carico da Codex"),
                            comment("@codex Decisione del responsabile: A"),
                            comment("Grazie!", "contributore")])
        self.assertIn("Decisione non ripresa", stuck(it, 60))

    def test_contributor_mention_is_not_a_start(self):
        it = item(comments=[comment("@claude prova", "contributore")])
        self.assertIsNone(stuck(it, 60))

    def test_resume_written_by_avvia_and_not_picked_up(self):
        it = item(labels={"agente:claude", "decisione"},
                  comments=[comment("Presa in carico da Claude"),
                            comment("@claude [modello: opus] [impegno: medio] Leggi AGENTS.md …")])
        self.assertIn("Decisione non ripresa", stuck(it, 60))

    def test_agent_name_without_mention_is_not_a_start(self):
        it = item(labels={"agente:codex", "decisione"},
                  comments=[comment("Presa in carico da Codex"),
                            comment("codex Decisione del responsabile: A")])
        self.assertIsNone(stuck(it, 60))


NOW = dt.datetime(2026, 11, 14, 20, 0, tzinfo=dt.timezone.utc)


def raw(number: int, minutes_ago: int, labels=(), body: str = "", pr: bool = False) -> dict:
    updated = (NOW - dt.timedelta(minutes=minutes_ago)).strftime("%Y-%m-%dT%H:%M:%SZ")
    r = {"number": number, "updated_at": updated, "body": body,
         "labels": [{"name": n} for n in labels]}
    if pr:
        r["pull_request"] = {}
    return r


class FakeGitHub:
    def __init__(self, comments: dict[int, list[dict]], running=()):
        self._comments, self._running = comments, set(running)

    def ci_running(self, number: int) -> bool:
        return number in self._running

    def comments(self, number: int) -> list[dict]:
        return list(self._comments.get(number, []))


def handover(number: int, at: str) -> dict:
    c = comment(f"@codex Prima di tutto esegui git fetch origin. Sei il revisore della pull request #{number} …")
    c["created_at"] = at
    return c


class Find(unittest.TestCase):
    def test_handover_in_the_issue_without_answer(self):
        # Claude opened PR 20, the CI passed, the hand-over to Codex went to
        # issue 10, and Codex never took it (quota).
        items = [raw(10, 90, {"agente:claude"}),
                 raw(20, 95, {}, "Closes #10", pr=True)]
        gh = FakeGitHub({10: [comment("Presa in carico da Claude"),
                              handover(20, "2026-11-14T18:30:00Z")]})
        found = find(items, NOW, 60, gh)
        self.assertEqual([it.number for it, _ in found], [20])
        self.assertIn("90 minuti", found[0][1])

    def test_handover_linked_by_the_start_line(self):
        items = [raw(10, 90, {"agente:codex"}),
                 raw(20, 95, {}, "Revisione della correzione: da fare\n\nAvvia dopo il merge: #10", pr=True)]
        gh = FakeGitHub({10: [handover(20, "2026-11-14T18:30:00Z")]})
        self.assertEqual([it.number for it, _ in find(items, NOW, 60, gh)], [20])

    def test_recent_movement_in_the_issue_keeps_the_chain_alive(self):
        items = [raw(10, 10, {"agente:claude"}),
                 raw(20, 95, {}, "Closes #10", pr=True)]
        gh = FakeGitHub({10: [handover(20, "2026-11-14T19:50:00Z")]})
        self.assertEqual(find(items, NOW, 60, gh), [])

    def test_running_ci_keeps_the_chain_alive(self):
        items = [raw(10, 90, {"agente:claude"}),
                 raw(20, 95, {}, "Closes #10", pr=True)]
        gh = FakeGitHub({10: [handover(20, "2026-11-14T18:30:00Z")]}, running={20})
        self.assertEqual(find(items, NOW, 60, gh), [])

    def test_quoted_start_line_does_not_link(self):
        items = [raw(10, 90, {"agente:claude"}),
                 raw(20, 95, {}, "Abbiamo tolto la riga Avvia dopo il merge: #10", pr=True)]
        gh = FakeGitHub({10: [comment("Presa in carico da Claude")]})
        self.assertEqual([it.number for it, _ in find(items, NOW, 60, gh)], [10])


if __name__ == "__main__":
    unittest.main()
