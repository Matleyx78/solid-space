#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The Tecnigrafo authors
# Copyright (C) 2026 The solid-space authors
# Adapted from Tecnigrafo commit a02fa2eac43af5a0aaccea28c22b68e9b3fabca3
#
# Hand a pull request over to the other agent of the chain
# (docs/agenti/catena.md; docs/adr/0018). The mention is written as the owner,
# because the agents answer only to the owner: never write it by hand.
#
#     scripts/passa.sh <claude|codex> <pr> revisione <round>
#     scripts/passa.sh <claude|codex> <pr> correzioni <round>
#     scripts/passa.sh <claude|codex> <pr> analisi
#
# Token: GH_TOKEN_UTENTE if set (Claude in GitHub Actions, the RELAY_PAT
# secret), otherwise GH_TOKEN (Codex, whose token already belongs to the
# owner), otherwise the one of `gh auth login` (the owner in a terminal).
#
# On a pull request, any review-like request to Codex starts its built-in code
# review instead of a task. Review and analysis requests for Codex therefore go
# to an issue: the one the pull request closes or, for a pull request that
# closes none (a correction of the specs, of references), the one of its line
# "Avvia dopo il merge: #N" (scripts/catena.py).
set -euo pipefail

repo="${GITHUB_REPOSITORY:-Matleyx78/solid-space}"

usage() {
  echo "usage: $0 <claude|codex> <pr> <revisione|correzioni|analisi> [round]" >&2
  exit 2
}

[ $# -ge 3 ] || usage
agent="$1" pr="$2" kind="$3" round="${4:-}"
case "$pr" in ''|*[!0-9]*) usage ;; esac

case "$agent" in
  claude) prefix="@claude [modello: opus] [impegno: alto]" ;;
  codex) prefix="@codex Prima di tutto esegui git fetch origin." ;;
  *) usage ;;
esac

case "$kind" in
  revisione|correzioni)
    case "$round" in ''|*[!0-9]*) usage ;; esac ;;
  analisi) ;;
  *) usage ;;
esac

case "$kind" in
  revisione)
    task="Rivedi questa pull request come dice docs/agenti/catena.md in \"Revisione (revisore)\" (giro $round)." ;;
  correzioni)
    task="Correggi le segnalazioni [bloccante] del giro di revisione $round (e della ripresa dopo la decisione, se c'è) come dice docs/agenti/catena.md in \"Correzioni (esecutore)\", e pubblica i commit con git push sullo stesso ramo." ;;
  analisi)
    task="La CI di questa pull request non passa dopo tre tentativi: analizzala come dice docs/agenti/catena.md in \"Analisi di una CI rossa (revisore)\"." ;;
esac

token="${GH_TOKEN_UTENTE:-${GH_TOKEN:-$(gh auth token 2>/dev/null || true)}}"
if [ -z "$token" ]; then
  echo "no token: set GH_TOKEN_UTENTE or GH_TOKEN, or run gh auth login" >&2
  exit 1
fi

target="$pr"
if [ "$agent" = codex ] && [ "$kind" != correzioni ]; then
  target="$(GH_TOKEN="$token" gh api "repos/$repo/pulls/$pr" --jq '.body // ""' \
    | python3 "$(dirname "$0")/catena.py" collegata)"
  if [ -z "$target" ]; then
    echo "pull request #$pr names no issue (Closes #N or Avvia dopo il merge: #N):" \
      "cannot hand it to Codex" >&2
    exit 1
  fi
  task="Sei il revisore della pull request #$pr: lavora sulla pull request #$pr, non su questa issue. $task"
  task="${task//questa pull request/la pull request #$pr}"
fi

text="$prefix Leggi AGENTS.md e seguilo alla lettera. $task"
GH_TOKEN="$token" gh api "repos/$repo/issues/$target/comments" -f body="$text" --jq '.html_url'
