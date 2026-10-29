#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."
preset=${SS_PRESET:-release}
build_dir="build${SS_BUILD_SUFFIX:-}/$preset"
only=
tidy=false
while (($#)); do
    case "$1" in
        --tidy) tidy=true; shift ;;
        --solo)
            [[ $# -ge 2 ]] || { echo '--solo requires a step' >&2; exit 2; }
            only=$2; shift 2 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done
case "$only" in
    ''|formato|configura|compila|test|tidy) ;;
    *) echo "Unknown step: $only" >&2; exit 2 ;;
esac
case "$preset" in release|debug|asan|tidy) ;; *) echo "Unknown preset: $preset" >&2; exit 2 ;; esac

log=$(mktemp)
trap 'rm -f "$log"' EXIT
run() {
    echo "==> $1"
    shift
    if "$@" >"$log" 2>&1; then
        tail -n 8 "$log"
    else
        local status=$?
        tail -n 80 "$log" >&2
        return "$status"
    fi
}
clang_tool() {
    local name=$1 candidate
    for candidate in "$name-18" "$name"; do
        if command -v "$candidate" >/dev/null &&
           "$candidate" --version | grep -Eq 'version 18\.'; then
            printf '%s\n' "$candidate"
            return
        fi
    done
    echo "Missing $name version 18; see docs/AMBIENTE-SVILUPPO.md" >&2
    return 1
}
format() {
    local tool
    tool=$(clang_tool clang-format) || return
    local -a files=()
    while IFS= read -r -d '' file; do
        [[ -f "$file" ]] && files+=("$file")
    done < <(git ls-files -z -- '*.cpp' '*.hpp' '*.h' '*.cc' '*.cxx')
    ((${#files[@]} == 0)) || "$tool" --dry-run --Werror "${files[@]}"
}
analyze() {
    local tool base
    tool=$(clang_tool clang-tidy) || return
    base=$(git merge-base "${SS_BASE_REF:-origin/main}" HEAD) || return
    SS_TIDY_BASE=$base SS_TIDY_TOOL=$tool SS_TIDY_BUILD=$build_dir python3 - <<'PY'
import json
import os
from pathlib import Path
import subprocess

def git_files(*args):
    return set(filter(None, subprocess.check_output(
        ['git', 'diff', '--name-only', '-z', '--diff-filter=ACMR', *args]
    ).decode().split('\0')))

base = os.environ['SS_TIDY_BASE']
changed = git_files(base, 'HEAD') | git_files('HEAD')
all_sources = any(p in changed for p in (
    '.clang-tidy', 'CMakeLists.txt', 'CMakePresets.json', 'scripts/controlla.sh'
)) or any(p.endswith(('.hpp', '.h', 'CMakeLists.txt', '.cmake')) for p in changed)
root = Path.cwd()
database = json.loads((Path(os.environ['SS_TIDY_BUILD']) / 'compile_commands.json').read_text())
sources = set()
for entry in database:
    path = Path(entry['file'])
    if not path.is_absolute():
        path = Path(entry['directory']) / path
    try:
        relative = path.resolve().relative_to(root).as_posix()
    except ValueError:
        continue
    if relative.split('/')[0] not in {'robust', 'geom2d', 'naming', 'solver', 'kernel', 'features', 'tests'}:
        continue
    if all_sources or relative in changed:
        sources.add(str(path.resolve()))
for source in sorted(sources):
    subprocess.run([os.environ['SS_TIDY_TOOL'], '-p', os.environ['SS_TIDY_BUILD'], source], check=True)
print(f'Analyzed {len(sources)} source files.')
PY
}
for step in formato configura compila test tidy; do
    [[ -z "$only" || "$only" == "$step" ]] || continue
    case "$step" in
        formato) run "$step" format ;;
        configura) run "$step" cmake --preset "$preset" ;;
        compila) run "$step" cmake --build "$build_dir" --parallel ;;
        test) run "$step" ctest --test-dir "$build_dir" --output-on-failure --no-tests=error ;;
        tidy)
            if $tidy || [[ "$only" == tidy ]]; then run "$step" analyze; fi ;;
    esac
done
