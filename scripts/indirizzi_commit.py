#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors
#
# Taken from scripts/indirizzi_commit.py of Matleyx78/tecnigrafo (GPL-3.0-or-later),
# same maintainer, at the maintainer's request.
#
# Checks that the commits of a pull request carry only noreply addresses
# (docs/DECISIONI.md, 2026-11-15, "Solo indirizzi noreply nei commit"). The squash and merge of
# GitHub copies the author of every commit into a Co-authored-by line of the
# commit on main, where it can no longer be removed. Run by the job "Commit
# addresses" of .github/workflows/ci.yml.
#
#     python3 scripts/indirizzi_commit.py <base> <head>
#
# For every commit in <base>..<head> it reads the address of the author, the
# one of the committer and those of the Co-authored-by lines of the message.
# Every address that is not allowed is printed masked (the log of the CI is
# public); the exit status is 1 if there is at least one, 0 otherwise.

from __future__ import annotations

import re
import subprocess
import sys
from dataclasses import dataclass

ALLOWED_SUFFIX = "@users.noreply.github.com"
ALLOWED = {"noreply@anthropic.com", "codex@openai.com", "noreply@github.com"}
CO_AUTHOR = re.compile(r"^\s*co-authored-by\s*:\s*(.*?)\s*$",
                       re.IGNORECASE | re.MULTILINE)
ANGLE = re.compile(r"<([^<>]*)>")
# Fields of git log separated by US and commits terminated by RS, so that the
# blank lines of a message never split a record.
FORMAT = "%h%x1f%ae%x1f%ce%x1f%B%x1e"


@dataclass(frozen=True)
class Problem:
    commit: str
    field: str
    address: str


def allowed(address: str) -> bool:
    a = address.strip().lower()
    return a.endswith(ALLOWED_SUFFIX) or a in ALLOWED


def mask(address: str) -> str:
    """Keeps only the first character and the domain: l…@example.com."""
    local, at, domain = address.strip().partition("@")
    return f"{local[:1]}…{at}{domain}"


def co_authors(message: str) -> list[str]:
    out = []
    for value in CO_AUTHOR.findall(message):
        m = ANGLE.search(value)
        out.append((m.group(1) if m else value).strip())
    return out


def parse(log: str) -> list[tuple[str, str, str, str]]:
    commits = []
    for record in log.split("\x1e"):
        record = record.lstrip("\n")
        if not record:
            continue
        sha, author, committer, message = record.split("\x1f", 3)
        commits.append((sha, author, committer, message))
    return commits


def check(log: str) -> list[Problem]:
    problems = []
    for sha, author, committer, message in parse(log):
        fields = [("author", author), ("committer", committer)]
        fields += [("Co-authored-by", a) for a in co_authors(message)]
        for name, address in fields:
            if not allowed(address):
                problems.append(Problem(sha, name, address))
    return problems


def read_log(base: str, head: str) -> str:
    return subprocess.run(
        ["git", "log", f"--format={FORMAT}", f"{base}..{head}"],
        check=True, capture_output=True, text=True, encoding="utf-8",
    ).stdout


def report(problems: list[Problem], base: str) -> str:
    lines = [f"::error title=Commit addresses::{p.commit} {p.field} "
             f"{mask(p.address)} is not a noreply address" for p in problems]
    lines += [
        "",
        "Only these addresses are allowed in the commits of a pull request:",
        f"  …{ALLOWED_SUFFIX}, " + ", ".join(sorted(ALLOWED)) + ".",
        "To fix the branch, set a noreply address and rewrite the commits:",
        "  git config user.email <id>+<name>@users.noreply.github.com",
        "  git rebase --exec 'git commit --amend --no-edit --reset-author' "
        f"{base}",
        "then remove or fix the Co-authored-by lines that are not allowed",
        "and publish with git push --force-with-lease.",
    ]
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(f"usage: {argv[0]} <base> <head>", file=sys.stderr)
        return 2
    base, head = argv[1], argv[2]
    problems = check(read_log(base, head))
    if not problems:
        print(f"Every commit in {base}..{head} has noreply addresses only.")
        return 0
    print(report(problems, base))
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
