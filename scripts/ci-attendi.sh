#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The Tecnigrafo authors
# Copyright (C) 2026 The solid-space authors
# Adapted from Tecnigrafo commit d52e169d21fedbc00b142a81895b5c89d0c7b403
#
# Wait for a CI run to finish, in ONE command that costs no tokens while it
# waits: the pauses happen inside the script, so an agent has nothing to poll.
#
#     scripts/ci-attendi.sh              # the run of the commit you are on
#     scripts/ci-attendi.sh <sha>        # the run of that commit
#     scripts/ci-attendi.sh <run id>     # that run
#
# Exit status:
#   0  the run succeeded
#   1  a check failed (read why with scripts/ci-errori.sh). The script does not
#      wait for the slow jobs (Windows, macOS) once one job has already failed
#   2  the GitHub API did not answer (the last error is printed)
#   3  the run is still going when the time ran out: run the same command again,
#      it resumes where it was
#   4  the run was cancelled: nothing failed, a newer push on the same branch
#      replaced it, so wait for that one
#   5  the time ran out SS_ATTESA_RILANCI times in a row on the same run (about
#      90 minutes in all, longer than any healthy run should take): do not
#      wait again, report it in the pull request (AGENTS.md, "Quando fermarsi")
#
# SS_ATTESA_MAX     seconds to wait before giving up (default 540: the Bash
#                   tool of an agent stops a foreground command at 600 s)
# SS_ATTESA_PAUSA   seconds between two checks (default 90: every check costs
#                   two requests and the anonymous limit of the GitHub API is 60
#                   an hour; GITHUB_TOKEN raises it)
# SS_ATTESA_RILANCI how many timeouts (exit 3) are allowed on one run (default 10: about 90 minutes in all)
#
# Needs jq, and either gh or curl (the repository is public: no token needed).
set -euo pipefail

repo=${SS_REPO:-Matleyx78/solid-space}
max=${SS_ATTESA_MAX:-540}
pause=${SS_ATTESA_PAUSA:-90}
max_timeouts=${SS_ATTESA_RILANCI:-10}

if ! command -v jq > /dev/null 2>&1; then
    echo "ci-attendi.sh needs jq." >&2
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
        echo "ci-attendi.sh needs gh or curl." >&2
        return 2
    fi
}

if (( $# > 1 )); then
    echo "usage: ci-attendi.sh [commit sha | run id]" >&2
    exit 2
fi
ref=${1:-$(git rev-parse HEAD)}

err=$(mktemp)
trap 'rm -f "$err"' EXIT

start=$SECONDS
errors=0
run=
status=
conclusion=

if [[ $ref =~ ^[0-9]{1,12}$ ]]; then
    run=$ref
fi

# Counts how many times in a row the wait ran out on this run.
timeouts_file() {
    printf '%s/ss-ci-attendi-%s.timeouts' "${TMPDIR:-/tmp}" "$run"
}

# Names of the jobs of the run that have already failed, even if the run goes on.
failed_jobs() {
    api "repos/$repo/actions/runs/$1/jobs?per_page=100" 2>> "$err" \
        | jq -r '.jobs[] | select(.conclusion == "failure" or .conclusion == "timed_out") | .name'
}

while :; do
    if reply=$( { if [[ -z $run ]]; then
                      api "repos/$repo/actions/workflows/ci.yml/runs?head_sha=$ref&per_page=1" \
                          | jq -c '.workflow_runs[0] // empty'
                  else
                      api "repos/$repo/actions/runs/$run" | jq -c '.'
                  fi; } 2> "$err" ); then
        errors=0
        if [[ -n $reply ]]; then
            run=$(jq -r '.id' <<< "$reply")
            status=$(jq -r '.status' <<< "$reply")
            conclusion=$(jq -r '.conclusion // "-"' <<< "$reply")
            if [[ $status == completed ]]; then
                rm -f "$(timeouts_file)"
                echo "run $run: completed $conclusion"
                [[ $conclusion == success ]] && exit 0
                if [[ $conclusion == cancelled ]]; then
                    echo "Nothing failed: a newer push on the same branch replaced this run."
                    echo "Run scripts/ci-attendi.sh again, on the newest commit."
                    exit 4
                fi
                exit 1
            fi
            # Still running: a job that already failed is enough to stop waiting.
            failed=$(failed_jobs "$run") || failed=
            if [[ -n $failed ]]; then
                rm -f "$(timeouts_file)"
                echo "run $run: still $status, but a check has already failed:"
                while IFS= read -r name; do
                    printf '  - %s\n' "$name"
                done <<< "$failed"
                echo "Do not wait for the other jobs: read why with scripts/ci-errori.sh $run"
                exit 1
            fi
        fi
    else
        # A network hiccup or the anonymous limit: try again a few times.
        errors=$((errors + 1))
        if (( errors >= 5 )); then
            echo "The GitHub API did not answer five times in a row. Last error:" >&2
            tail -n 3 "$err" >&2
            if grep -qE '403|429|rate limit' "$err"; then
                echo "It looks like the anonymous request limit: set GITHUB_TOKEN, or wait." >&2
            fi
            exit 2
        fi
    fi
    if (( SECONDS - start + pause > max )); then
        if [[ -z $run ]]; then
            echo "No CI run for $ref after $((SECONDS - start)) s: it has not started, or the commit was not pushed." >&2
            exit 1
        fi
        count=$(( $(cat "$(timeouts_file)" 2> /dev/null || echo 0) + 1 ))
        echo "$count" > "$(timeouts_file)"
        if (( count >= max_timeouts )); then
            echo "run $run: still ${status:-unknown} after $count waits in a row. Do not wait again:"
            echo "write in the pull request what you saw (AGENTS.md, \"Quando fermarsi\"), remove the label, end the session."
            exit 5
        fi
        echo "run $run: still ${status:-unknown} after $((SECONDS - start)) s (wait $count of $max_timeouts). Run the same command again."
        exit 3
    fi
    sleep "$pause"
done
