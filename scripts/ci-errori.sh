#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The Tecnigrafo authors
# Copyright (C) 2026 The solid-space authors
# Adapted from Tecnigrafo commit d52e169d21fedbc00b142a81895b5c89d0c7b403
#
# Print why a CI run failed, in a few lines: for every failed job its name, the
# failed step and the error annotations (see scripts/ci-annota.sh). It reads
# the REST API only, so it works where the raw logs cannot be downloaded, and
# it leaves out warnings, which are not the cause of a failure.
#
#     scripts/ci-errori.sh              # the run of the commit you are on
#     scripts/ci-errori.sh <sha>        # the run of that commit
#     scripts/ci-errori.sh <pr number>  # the run of the last commit of that pull request
#     scripts/ci-errori.sh <run id>     # that run
#
# A number of up to 6 digits is a pull request, a longer one a run id (run ids
# have 10 digits and more).
#
# Needs jq, and either gh or curl (the repository is public, so curl works
# without a token; set GITHUB_TOKEN to raise the anonymous limit of 60
# requests an hour). Wait for the run to finish first (AGENTS.md).
#
# The error annotations are read without the personal token of the chain of
# agents (docs/agenti/catena.md), whose permissions do not include checks:
# with GH_TOKEN_LETTURA (the job token that .github/workflows/claude.yml
# passes, with checks: read) where it is set, otherwise with no token at all,
# since the repository is public (Codex, a terminal).
set -euo pipefail

repo=${SS_REPO:-Matleyx78/solid-space}

if ! command -v jq > /dev/null 2>&1; then
    echo "ci-errori.sh needs jq." >&2
    exit 2
fi

# Print the JSON the REST API returns for a path such as repos/<repo>/...
api() {
    if command -v gh > /dev/null 2>&1; then
        gh api "$1"
    elif command -v curl > /dev/null 2>&1; then
        curl --connect-timeout 10 --max-time 30 -fsS -H 'Accept: application/vnd.github+json' \
            ${GITHUB_TOKEN:+-H "Authorization: Bearer $GITHUB_TOKEN"} \
            "https://api.github.com/$1"
    else
        echo "ci-errori.sh needs gh or curl." >&2
        return 2
    fi
}

# The error annotations of a check run, for a path such as
# repos/<repo>/check-runs/<id>/annotations.
annotations() {
    if [[ -n ${GH_TOKEN_LETTURA:-} ]]; then
        if command -v gh > /dev/null 2>&1; then
            GH_TOKEN=$GH_TOKEN_LETTURA gh api "$1"
        else
            curl --connect-timeout 10 --max-time 30 -fsS -H 'Accept: application/vnd.github+json' \
                -H "Authorization: Bearer $GH_TOKEN_LETTURA" \
                "https://api.github.com/$1"
        fi
    elif command -v curl > /dev/null 2>&1; then
        curl --connect-timeout 10 --max-time 30 -fsS -H 'Accept: application/vnd.github+json' \
            "https://api.github.com/$1"
    else
        api "$1"
    fi
}

if (( $# > 1 )); then
    echo "usage: ci-errori.sh [commit sha | pull request number | run id]" >&2
    exit 2
fi
ref=${1:-$(git rev-parse HEAD)}

if [[ $ref =~ ^[0-9]{1,6}$ ]]; then
    # A pull request number: its last commit.
    pr=$ref
    ref=$(api "repos/$repo/pulls/$pr" | jq -r '.head.sha // empty')
    if [[ -z $ref ]]; then
        echo "No pull request #$pr in $repo." >&2
        exit 2
    fi
    echo "pull request #$pr: commit $ref"
fi

if [[ $ref =~ ^[0-9]{7,}$ ]]; then
    run=$ref
else
    # The newest run of the commit; several appear after a re-run.
    run=$(api "repos/$repo/actions/workflows/ci.yml/runs?head_sha=$ref&per_page=1" \
        | jq -r '.workflow_runs[0].id // empty')
    if [[ -z $run ]]; then
        echo "No CI run for $ref yet: it has not started, or the commit was not pushed." >&2
        exit 1
    fi
fi

summary=$(api "repos/$repo/actions/runs/$run" \
    | jq -r '"run \(.id): \(.status) \(.conclusion // "-")  \(.html_url)"')
echo "$summary"
if [[ $summary == *" completed cancelled "* ]]; then
    echo "Nothing failed: a newer push on the same branch replaced this run."
    echo "Look at the run of the newest commit: scripts/ci-errori.sh <sha>."
    exit 0
fi

jobs=$(api "repos/$repo/actions/runs/$run/jobs?per_page=100" | jq -r '
    .jobs[] | select(.conclusion == "failure" or .conclusion == "timed_out")
    | "\(.id)\t\(.name)\t\([.steps[] | select(.conclusion == "failure" or .conclusion == "timed_out") | .name] | join(", "))"')

if [[ -z $jobs ]]; then
    echo "No failed job (yet)."
    exit 0
fi

while IFS=$'\t' read -r id name step; do
    echo
    echo "== $name (job $id): failed step: ${step:-?}"
    annotations "repos/$repo/check-runs/$id/annotations?per_page=50" | jq -r '
        .[] | select(.annotation_level == "failure")
        | select(.message | test("^Process completed with exit code") | not)
        | "\(if .path == ".github" then "" else "\(.path):\(.start_line) " end)\(if (.title // "") == "" then "" else "[\(.title)] " end)\(.message | gsub("\n"; " | ") | .[0:400])"' \
        || true
done <<< "$jobs"
