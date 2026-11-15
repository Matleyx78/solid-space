#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The Tecnigrafo authors
# Copyright (C) 2026 The solid-space authors
# Adapted from Tecnigrafo commit a02fa2eac43af5a0aaccea28c22b68e9b3fabca3
#
# Start (or resume) the work on an issue of the chain, writing the mention as
# the owner (docs/agenti/catena.md, "Dopo il merge"; docs/adr/0018). The agent
# is the one of the issue's "agente:" label (agente:claude-2 is Claude too).
# For Claude, model and effort come from the "**Modello e impegno:**" line of
# the issue (docs/LAVORO-IN-PARALLELO.md, "Modello e impegno"), for example
# "medio (Opus 5.5 medio); impegno alto": the model is the Claude model named
# in it (opus when none), the effort the word after "impegno" (medio when
# missing).
#
#     scripts/avvia.sh <issue> [reason]
#
# The issue is not started, and the script says why, when it is closed, when
# an agent holds it (AA-Presa in carico) or when an issue of its
# "**Dipende da:**" or "**Riferimenti:**" lines is still open (also in another
# repository, written owner/repo#N).
#
# With DRY_RUN=1 it only prints the comment it would write.
#
# Token: GH_TOKEN_UTENTE if set (the RELAY_PAT secret), otherwise GH_TOKEN,
# otherwise the one of `gh auth login` (the owner in a terminal).
# The agents answer only to the owner, so the comment must use the owner's
# token; the reads use the same one.
set -euo pipefail

repo="${GITHUB_REPOSITORY:-Matleyx78/solid-space}"
issue="${1:-}" reason="${2:-}"
case "$issue" in ''|*[!0-9]*) echo "usage: $0 <issue> [reason]" >&2; exit 2 ;; esac

token="${GH_TOKEN_UTENTE:-${GH_TOKEN:-$(gh auth token 2>/dev/null || true)}}"
[ -n "$token" ] || { echo "no token: set GH_TOKEN_UTENTE or GH_TOKEN, or run gh auth login" >&2; exit 1; }
export GH_TOKEN="$token"

data="$(gh api "repos/$repo/issues/$issue")"
state="$(jq -r '.state' <<<"$data")"
labels="$(jq -r '.labels[].name' <<<"$data")"
body="$(jq -r '.body // ""' <<<"$data")"

if [ "$state" != open ]; then
  echo "issue #$issue is $state: not started"
  exit 0
fi
if grep -Fxq "AA-Presa in carico" <<<"$labels"; then
  echo "issue #$issue is held by an agent (AA-Presa in carico): not started"
  exit 0
fi

agent="$(grep -m1 -E '^agente:(claude|claude-2|codex)$' <<<"$labels" | sed -E 's/^agente://; s/-2$//' || true)"

# Dependencies: every #N or owner/repo#N of the two lines must be closed.
waiting=()
deps="$(grep -E '^\*\*(Dipende da|Riferimenti):\*\*' <<<"$body" \
  | grep -oE '([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+)?#[0-9]+' | sort -u || true)"
for ref in $deps; do
  dep_repo="${ref%%#*}"
  dep_repo="${dep_repo:-$repo}"
  dep_state="$(gh api "repos/$dep_repo/issues/${ref##*#}" --jq '.state' 2>/dev/null || echo unknown)"
  [ "$dep_state" = closed ] || waiting+=("$ref ($dep_state)")
done
if [ ${#waiting[@]} -gt 0 ]; then
  echo "issue #$issue waits for: ${waiting[*]}: not started"
  exit 0
fi

case "$agent" in
  claude)
    line="$(grep -m1 -E '\*\*Modello e impegno:\*\*' <<<"$body" | sed -E 's/.*\*\*Modello e impegno:\*\*//' || true)"
    model="$(grep -oiE '\b(opus|sonnet|haiku)\b' <<<"$line" | head -n1 | tr '[:upper:]' '[:lower:]' || true)"
    effort="$(grep -oE '\bimpegno[[:space:]]+(basso|medio|alto|molto alto|massimo)\b' <<<"$line" \
      | tail -n1 | sed -E 's/^impegno[[:space:]]+//' || true)"
    prefix="@claude [modello: ${model:-opus}] [impegno: ${effort:-medio}]" ;;
  codex)
    prefix="@codex Prima di tutto esegui git fetch origin." ;;
  *)
    echo "issue #$issue has no agente:claude or agente:codex label" >&2
    exit 1 ;;
esac

text="$prefix Leggi AGENTS.md e seguilo alla lettera. ${reason:+$reason }Realizza questa issue, o riprendila da dove si era fermata, come dice docs/agenti/catena.md. Il lavoro esiste solo su GitHub: pubblica il ramo con git push e apri tu la pull request con gh pr create."
if [ "${DRY_RUN:-0}" = 1 ]; then
  echo "issue #$issue, would write: $text"
  exit 0
fi

# The label decisione stays: the agent removes it when it takes the issue in
# charge (docs/agenti/catena.md, "Ripresa dopo una decisione"), so a start
# that fails or is never picked up leaves the issue marked.
gh api "repos/$repo/issues/$issue/comments" -f body="$text" --jq '.html_url'
