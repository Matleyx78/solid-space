# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors
"""Structural validator for the verification corpus (ADR 0005, point 5).

It checks the shape of the cases, of sorgenti.json and of the files in in-attesa/,
and the presence and SHA-256 of the mandatory local copies of external files. It never
downloads anything and never runs a case against the code. Standard library only.

Usage: python3 valida_corpus.py [CORPUS_DIR]   (default: the directory of this file)
Exit status: 0 valid, 1 problems found (printed on stderr), 2 usage error.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path

FORMAT_VERSION = 1
COMPONENTS = ("robust", "geom2d", "naming", "solver", "kernel", "features")
CASE_FIELDS = ("versione", "id", "componente", "descrizione", "ingresso", "atteso",
               "provenienza")
SOURCE_FIELDS = ("nome", "indirizzo", "versione", "sha256", "licenza")
# Each provenance type and the key that documents it.
PROVENANCE_KEYS = {"calcolo-a-mano": "procedimento", "aritmetica-esatta": "procedimento",
                   "file-pubblico": "sorgente"}
# SPDX values that mean "unknown": a file with an unknown licence cannot enter.
UNKNOWN_LICENSES = {"NOASSERTION", "NONE"}

_WORDS = r"[a-z0-9]+(?:-[a-z0-9]+)*"
NAME_RE = re.compile(_WORDS)
PENDING_RE = re.compile(rf"({_WORDS})/({_WORDS}) #([1-9][0-9]*)")
PENDING_DIR = "in-attesa"
# One file per block of references, named after the tag of its package: 0.2-R4a.txt.
PENDING_FILE_RE = re.compile(r"[0-9]+\.[0-9]+-R[0-9]+[a-z]?\.txt")
FRACTION_RE = re.compile(r"-?(?:0|[1-9][0-9]*)(?:/[1-9][0-9]*)?")
ENUM_NAME_RE = re.compile(r"[a-z][A-Za-z0-9]*")
ZERO_DENOMINATOR_RE = re.compile(r"-?[0-9]+/0+")
SHA256_RE = re.compile(r"[0-9a-f]{64}")
FILE_NAME_RE = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]*")
LICENSE_RE = re.compile(r"[A-Za-z0-9.+-]+(?: (?:AND|OR|WITH) [A-Za-z0-9.+-]+)*")
URL_RE = re.compile(r"https://[^\s/]+\S*")


@dataclass(frozen=True)
class Problem:
    """One thing wrong in the corpus: which file, which field, and why."""

    file: str
    field: str
    message: str

    def __str__(self) -> str:
        return f"{self.file}: {self.field}: {self.message}"


# --- helpers -----------------------------------------------------------------

def _kind(value) -> str:
    if value is None:
        return "null"
    if isinstance(value, bool):
        return "boolean"
    if isinstance(value, (int, float)):
        return "number"
    return {str: "string", list: "array", dict: "object"}[type(value)]


def _is_number(value) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _is_finite_number(value) -> bool:
    # Integers are always finite; math.isfinite would overflow on huge ones.
    return _is_number(value) and (not isinstance(value, float) or math.isfinite(value))


def _unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate key {key!r}")
        result[key] = value
    return result


def _reject_constant(token):
    raise ValueError(f"{token} is not valid JSON")


def load_json(path: Path, name: str):
    """Parse a JSON file strictly (no duplicate keys, no NaN/Infinity)."""
    try:
        text = Path(path).read_text(encoding="utf-8")
        return json.loads(text, object_pairs_hook=_unique_pairs,
                          parse_constant=_reject_constant), []
    except (OSError, ValueError, RecursionError) as err:
        return None, [Problem(name, "(file)", f"cannot read as JSON: {err}")]


def _text_problem(value, field, name, out) -> bool:
    if isinstance(value, str) and value.strip():
        return False
    out.append(Problem(name, field, f"expected a non-empty string, found {_kind(value)}"
                       if not isinstance(value, str) else "must not be empty"))
    return True


def _sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


# --- one case ----------------------------------------------------------------

def _check_approximate(obj, field, name, out):
    for key in sorted(set(obj) - {"valore", "tolleranza"}):
        out.append(Problem(name, f"{field}.{key}",
                           'unknown key (an approximate value has only "valore" and '
                           '"tolleranza")'))
    for key in ("valore", "tolleranza"):
        if key not in obj:
            out.append(Problem(name, f"{field}.{key}", "required key is missing"))
        elif not _is_finite_number(obj[key]):
            out.append(Problem(name, f"{field}.{key}", "expected a finite number"))
        elif key == "tolleranza" and obj[key] <= 0:
            out.append(Problem(name, f"{field}.{key}", "tolerance must be greater than zero"))


def _walk(value, field, name, strict, out):
    """Check the values of "ingresso" (loose) or "atteso" (strict)."""
    if value is None:
        out.append(Problem(name, field, "null is not allowed"))
    elif isinstance(value, bool):
        return
    elif _is_number(value):
        if not _is_finite_number(value):
            out.append(Problem(name, field, "number is not finite"))
        elif strict:
            out.append(Problem(name, field,
                               'bare number: write an exact value as a fraction string '
                               '("1/3") or an approximate one as {"valore": ..., '
                               '"tolleranza": ...}'))
    elif isinstance(value, str):
        if strict and not (FRACTION_RE.fullmatch(value) or ENUM_NAME_RE.fullmatch(value)):
            reason = ("denominator is zero" if ZERO_DENOMINATOR_RE.fullmatch(value) else
                      'expected a fraction string such as "1/3" or "6", or an ASCII name '
                      'matching [a-z][A-Za-z0-9]*')
            out.append(Problem(name, field, reason))
    elif isinstance(value, list):
        for index, item in enumerate(value):
            _walk(item, f"{field}[{index}]", name, strict, out)
    elif strict and ("valore" in value or "tolleranza" in value):
        _check_approximate(value, field, name, out)
    else:
        for key, item in value.items():
            _walk(item, f"{field}.{key}", name, strict, out)


def _check_provenance(prov, name, source_keys, out):
    if not isinstance(prov, dict):
        out.append(Problem(name, "provenienza", f"expected an object, found {_kind(prov)}"))
        return
    kind = prov.get("tipo")
    if not isinstance(kind, str) or kind not in PROVENANCE_KEYS:
        out.append(Problem(name, "provenienza.tipo",
                           "must be one of " + ", ".join(PROVENANCE_KEYS)))
        return
    for key in sorted(set(prov) - {"tipo", "procedimento", "sorgente"}):
        out.append(Problem(name, f"provenienza.{key}", "unknown key"))
    needed = PROVENANCE_KEYS[kind]
    if needed not in prov:
        out.append(Problem(name, f"provenienza.{needed}",
                           f'required when tipo is "{kind}"'))
    if "procedimento" in prov:
        _text_problem(prov["procedimento"], "provenienza.procedimento", name, out)
    if "sorgente" in prov:
        if kind != "file-pubblico":
            out.append(Problem(name, "provenienza.sorgente",
                               'only allowed when tipo is "file-pubblico"'))
        elif not isinstance(prov["sorgente"], str) or not NAME_RE.fullmatch(prov["sorgente"]):
            out.append(Problem(name, "provenienza.sorgente", "expected a key of sorgenti.json"))
        elif source_keys is not None and prov["sorgente"] not in source_keys:
            out.append(Problem(name, "provenienza.sorgente",
                               f'no entry "{prov["sorgente"]}" in sorgenti.json'))


def check_case(data, name: str, stem: str, component_dir: str | None = None,
               source_keys=None) -> list[Problem]:
    """Validate a parsed case. `stem` is the file name without extension and
    `component_dir` the directory it sits in (None: do not check it)."""
    if not isinstance(data, dict):
        return [Problem(name, "(file)", f"expected an object, found {_kind(data)}")]
    out: list[Problem] = []
    for field in CASE_FIELDS:
        if field not in data:
            out.append(Problem(name, field, "required field is missing"))
    for key in sorted(set(data) - set(CASE_FIELDS)):
        out.append(Problem(name, key, "unknown field"))
    if "versione" in data and not (type(data["versione"]) is int
                                   and data["versione"] == FORMAT_VERSION):
        out.append(Problem(name, "versione", f"must be the integer {FORMAT_VERSION}"))
    if "id" in data and (not isinstance(data["id"], str)
                         or not NAME_RE.fullmatch(data["id"])):
        out.append(Problem(name, "id", "must be lowercase words joined by hyphens"))
    elif "id" in data and data["id"] != stem:
        out.append(Problem(name, "id", f'must equal the file name without extension ("{stem}")'))
    if "componente" in data:
        component = data["componente"]
        if not isinstance(component, str) or component not in COMPONENTS:
            out.append(Problem(name, "componente", "must be one of " + ", ".join(COMPONENTS)))
        elif component_dir is not None and component != component_dir:
            out.append(Problem(name, "componente",
                               f'must equal the directory it is in ("{component_dir}")'))
    if "descrizione" in data:
        _text_problem(data["descrizione"], "descrizione", name, out)
    for field, strict in (("ingresso", False), ("atteso", True)):
        if field not in data:
            continue
        if not isinstance(data[field], dict) or not data[field]:
            out.append(Problem(name, field, "expected a non-empty object"))
            continue
        for key, value in data[field].items():
            _walk(value, f"{field}.{key}", name, strict, out)
    if "provenienza" in data:
        _check_provenance(data["provenienza"], name, source_keys, out)
    return out


def validate_case_file(path, name: str | None = None, expected_component: str | None = None,
                       source_keys=None) -> list[Problem]:
    path = Path(path)
    name = name or path.name
    data, problems = load_json(path, name)
    if problems:
        return problems
    return check_case(data, name, path.stem, expected_component, source_keys)


# --- sorgenti.json -----------------------------------------------------------

def check_sources(data, name: str, base_dir=None) -> list[Problem]:
    """Validate the parsed sorgenti.json. If `base_dir` is given, a local copy at
    <base_dir>/esterni/<nome> is required and must match the declared SHA-256.
    Every file under esterni/ must also have a source entry."""
    if not isinstance(data, dict):
        return [Problem(name, "(file)", f"expected an object, found {_kind(data)}")]
    out: list[Problem] = []
    for field in ("versione", "sorgenti"):
        if field not in data:
            out.append(Problem(name, field, "required field is missing"))
    for key in sorted(set(data) - {"versione", "sorgenti"}):
        out.append(Problem(name, key, "unknown field"))
    if "versione" in data and not (type(data["versione"]) is int
                                   and data["versione"] == FORMAT_VERSION):
        out.append(Problem(name, "versione", f"must be the integer {FORMAT_VERSION}"))
    entries = data.get("sorgenti", {})
    if not isinstance(entries, dict):
        return out + [Problem(name, "sorgenti", f"expected an object, found {_kind(entries)}")]
    names_seen: dict[str, str] = {}
    for key, entry in entries.items():
        field = f"sorgenti.{key}"
        if not NAME_RE.fullmatch(key):
            out.append(Problem(name, field, "key must be lowercase words joined by hyphens"))
        if not isinstance(entry, dict):
            out.append(Problem(name, field, f"expected an object, found {_kind(entry)}"))
            continue
        for sub in SOURCE_FIELDS:
            if sub not in entry:
                out.append(Problem(name, f"{field}.{sub}", "required field is missing"))
        for sub in sorted(set(entry) - set(SOURCE_FIELDS)):
            out.append(Problem(name, f"{field}.{sub}", "unknown field"))
        _check_source_values(entry, field, name, out)
        file_name, declared = entry.get("nome"), entry.get("sha256")
        if isinstance(file_name, str) and FILE_NAME_RE.fullmatch(file_name):
            if file_name in names_seen:
                out.append(Problem(name, f"{field}.nome",
                                   f'same file name as "{names_seen[file_name]}"'))
            names_seen.setdefault(file_name, key)
            if base_dir is not None and isinstance(declared, str) \
                    and SHA256_RE.fullmatch(declared):
                _check_local_copy(Path(base_dir) / "esterni" / file_name, declared,
                                  f"{field}.sha256", name, out)
    if base_dir is not None:
        external_dir = Path(base_dir) / "esterni"
        if external_dir.is_dir():
            for local in sorted(external_dir.rglob("*")):
                if local.is_file():
                    relative = local.relative_to(external_dir).as_posix()
                    if relative not in names_seen:
                        out.append(Problem(name, "sorgenti",
                                           f"esterni/{relative} has no entry in sorgenti.json"))
    return out


def _check_source_values(entry, field, name, out):
    def bad(sub, message):
        out.append(Problem(name, f"{field}.{sub}", message))

    if "nome" in entry and not (isinstance(entry["nome"], str)
                                and FILE_NAME_RE.fullmatch(entry["nome"])):
        bad("nome", "expected a plain file name (letters, digits, . _ -)")
    if "indirizzo" in entry and not (isinstance(entry["indirizzo"], str)
                                     and URL_RE.fullmatch(entry["indirizzo"])):
        bad("indirizzo", "expected an https:// address")
    if "versione" in entry:
        _text_problem(entry["versione"], f"{field}.versione", name, out)
    if "sha256" in entry and not (isinstance(entry["sha256"], str)
                                  and SHA256_RE.fullmatch(entry["sha256"])):
        bad("sha256", "expected 64 lowercase hexadecimal digits")
    if "licenza" in entry and not (isinstance(entry["licenza"], str)
                                   and LICENSE_RE.fullmatch(entry["licenza"])
                                   and entry["licenza"] not in UNKNOWN_LICENSES):
        bad("licenza", "expected a known SPDX identifier (not NOASSERTION or NONE)")


def _check_local_copy(local: Path, declared: str, field: str, name: str, out):
    if not local.exists():
        out.append(Problem(name, field, f"esterni/{local.name} is missing"))
        return
    if not local.is_file():
        out.append(Problem(name, field, f"{local.name} in esterni/ is not a regular file"))
        return
    actual = _sha256_of(local)
    if actual != declared:
        out.append(Problem(name, field, f"esterni/{local.name} has SHA-256 {actual}, "
                                        f"declared {declared}"))


def validate_sources_file(path, name: str | None = None) -> list[Problem]:
    path = Path(path)
    name = name or path.name
    data, problems = load_json(path, name)
    return problems or check_sources(data, name, path.parent)


# --- in-attesa/ --------------------------------------------------------------

def load_pending_file(path, known_cases, name: str | None = None):
    """One file of the pending list. Each line is "<componente>/<id> #<issue>";
    `known_cases` holds the "<componente>/<id>" of the cases that exist. Blank lines
    and "#" comments are ignored."""
    path = Path(path)
    name = name or path.name
    try:
        text = path.read_text(encoding="utf-8-sig")
    except (OSError, ValueError) as err:
        return {}, [Problem(name, "(file)", f"cannot read file: {err}")]
    out: list[Problem] = []
    seen = {}
    for number, raw in enumerate(text.splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        field = f"line {number}"
        match = PENDING_RE.fullmatch(line)
        if not match:
            out.append(Problem(name, field, 'expected "<componente>/<id> #<issue number>"'))
            continue
        case = f"{match.group(1)}/{match.group(2)}"
        if match.group(1) not in COMPONENTS:
            out.append(Problem(name, field, f"unknown component in {case}"))
        elif case not in known_cases:
            out.append(Problem(name, field, f"case {case} does not exist under casi/"))
        if case in seen:
            out.append(Problem(name, field, f"case {case} is listed twice"))
        seen[case] = int(match.group(3))
    return seen, out


def validate_pending_file(path, known_cases, name: str | None = None) -> list[Problem]:
    return load_pending_file(path, known_cases, name)[1]


def load_pending_dir(directory, known_cases, name: str = PENDING_DIR):
    """The pending list: one file per block of references, named after its tag
    ("0.2-R4a.txt"), each with the lines of `load_pending_file`. A case is listed in
    one file only, and a file with no case is a leftover to delete. Names that start
    with "." (the .gitkeep that keeps the directory in git) are ignored. Two packages
    never touch the same file, so the list needs no merge."""
    directory = Path(directory)
    try:
        entries = sorted(p for p in directory.iterdir() if not p.name.startswith("."))
    except OSError as err:
        return {}, [Problem(name, "(file)", f"cannot read directory: {err}")]
    pending: dict[str, int] = {}
    owner: dict[str, str] = {}
    out: list[Problem] = []
    for path in entries:
        file_name = f"{name}/{path.name}"
        if not path.is_file() or not PENDING_FILE_RE.fullmatch(path.name):
            out.append(Problem(file_name, "(file)",
                               'expected a file named "<X.Y>-R<N>[a-z].txt", after the tag '
                               "of its block of references"))
            continue
        seen, problems = load_pending_file(path, known_cases, file_name)
        out += problems
        if not seen and not problems:
            out.append(Problem(file_name, "(file)",
                               "lists no case: delete the file when its cases become mandatory"))
        for case, issue in seen.items():
            if case in pending:
                out.append(Problem(file_name, "(file)",
                                   f"case {case} is also listed in {owner[case]}"))
            else:
                pending[case] = issue
                owner[case] = file_name
    return pending, out


def validate_pending_dir(directory, known_cases, name: str = PENDING_DIR) -> list[Problem]:
    return load_pending_dir(directory, known_cases, name)[1]


# --- the whole corpus --------------------------------------------------------

def validate_corpus(root) -> list[Problem]:
    root = Path(root)
    out: list[Problem] = []
    source_keys = None
    if (root / "sorgenti.json").is_file():
        data, problems = load_json(root / "sorgenti.json", "sorgenti.json")
        out += problems or check_sources(data, "sorgenti.json", root)
        if isinstance(data, dict) and isinstance(data.get("sorgenti"), dict):
            source_keys = set(data["sorgenti"])
    else:
        out.append(Problem("sorgenti.json", "(file)", "file is missing"))

    known: set[str] = set()
    cases_dir = root / "casi"
    if cases_dir.is_dir():
        for comp_dir in sorted(p for p in cases_dir.iterdir() if not p.name.startswith(".")):
            if not comp_dir.is_dir() or comp_dir.name not in COMPONENTS:
                out.append(Problem(f"casi/{comp_dir.name}", "(file)",
                                   "expected a directory named after a component"))
                continue
            for case in sorted(p for p in comp_dir.iterdir() if not p.name.startswith(".")):
                name = f"casi/{comp_dir.name}/{case.name}"
                if not case.is_file() or case.suffix != ".json":
                    out.append(Problem(name, "(file)", "expected a .json case file"))
                    continue
                known.add(f"{comp_dir.name}/{case.stem}")
                out += validate_case_file(case, name, comp_dir.name, source_keys)
    else:
        out.append(Problem("casi", "(file)", "directory is missing"))

    if (root / "in-attesa.txt").exists():
        out.append(Problem("in-attesa.txt", "(file)",
                           "the single list was replaced by the directory in-attesa/: "
                           "move each block to a file named after its tag"))
    if (root / PENDING_DIR).is_dir():
        out += validate_pending_dir(root / PENDING_DIR, known)
    else:
        out.append(Problem(PENDING_DIR, "(file)", "directory is missing"))
    return out


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="Validate the verification corpus.")
    parser.add_argument("corpus", nargs="?", type=Path,
                        default=Path(__file__).resolve().parent,
                        help="corpus directory (default: the directory of this script)")
    args = parser.parse_args(argv)
    if not args.corpus.is_dir():
        print(f"{args.corpus}: not a directory", file=sys.stderr)
        return 2
    problems = validate_corpus(args.corpus)
    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        print(f"{len(problems)} problem(s) found in {args.corpus}", file=sys.stderr)
        return 1
    print(f"corpus valid: {args.corpus}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
