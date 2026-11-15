#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The Tecnigrafo authors
# Copyright (C) 2026 The solid-space authors
# Adapted from Tecnigrafo commit a02fa2eac43af5a0aaccea28c22b68e9b3fabca3
#
# The lines of a pull request description that the chain of agents reads
# (docs/agenti/catena.md; docs/adr/0018), in one place for the workflow
# "Dopo il merge", scripts/passa.sh and scripts/sentinella.py.
#
#     python3 scripts/catena.py avvia < description      # one N per line
#     python3 scripts/catena.py collegata < description  # one N, or nothing
#
# "Avvia dopo il merge: #N" counts only as a line of its own: the phrase at
# the start of the line (after spaces or a list mark), one issue, nothing
# after it. The same words inside a sentence are a quotation, not an order.
# Lines inside fenced code blocks (``` or ~~~) are examples and never count,
# for "Closes #N" either.

from __future__ import annotations

import re
import sys

AVVIA = re.compile(r"^[ \t]*(?:[-*][ \t]+)?Avvia dopo il merge:[ \t]*#(\d+)[ \t\r]*$",
                   re.IGNORECASE | re.MULTILINE)
CLOSES = re.compile(r"\b(?:closes|fixes|resolves)\s+#(\d+)", re.IGNORECASE)
FENCE = re.compile(r"^ {0,3}(`{3,}|~{3,})")


def fuori_dal_codice(text: str) -> str:
    """The text without the lines of fenced code blocks, fences included. As in
    CommonMark, a block closes with a fence of the same character at least as
    long, with nothing after it, and a block never closed runs to the end."""
    out = []
    fence = ""
    for line in (text or "").splitlines():
        if fence:
            m = FENCE.match(line)
            if (m and m.group(1)[0] == fence[0] and len(m.group(1)) >= len(fence)
                    and not line[m.end():].strip()):
                fence = ""
            continue
        m = FENCE.match(line)
        if m and not (m.group(1)[0] == "`" and "`" in line[m.end():]):
            fence = m.group(1)
            continue
        out.append(line)
    return "\n".join(out)


def avvia_dopo_il_merge(text: str) -> list[int]:
    """The issues of the lines "Avvia dopo il merge: #N", sorted, no repeats."""
    return sorted({int(n) for n in AVVIA.findall(fuori_dal_codice(text))})


def chiude(text: str) -> list[int]:
    """The issues that the description closes, in order of appearance."""
    return [int(n) for n in CLOSES.findall(fuori_dal_codice(text))]


def collegate(text: str) -> list[int]:
    """The issues of a pull request in the chain: the ones it closes, then the
    ones of its lines "Avvia dopo il merge" (a correction of specs or
    references closes none)."""
    out: list[int] = []
    for n in chiude(text) + avvia_dopo_il_merge(text):
        if n not in out:
            out.append(n)
    return out


def main(argv: list[str]) -> int:
    if len(argv) != 2 or argv[1] not in ("avvia", "collegata"):
        print(f"usage: {argv[0]} avvia|collegata < description", file=sys.stderr)
        return 2
    text = sys.stdin.read()
    numbers = avvia_dopo_il_merge(text) if argv[1] == "avvia" else collegate(text)[:1]
    for n in numbers:
        print(n)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
