#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors
"""Validate corpus completeness and compare system directories byte for byte."""

import argparse
import difflib
from pathlib import Path
import re
import sys


def read_file(directory, name):
    try:
        return (directory / name).read_bytes()
    except OSError as error:
        raise ValueError(f'{directory.name}: {name}: {error}') from error


def validate(directory):
    required_bytes = read_file(directory, 'obbligatori.txt')
    result_bytes = read_file(directory, 'risultati.txt')
    try:
        required = required_bytes.decode('utf-8').splitlines()
        results = result_bytes.decode('utf-8').splitlines()
    except UnicodeDecodeError as error:
        raise ValueError(f'{directory.name}: invalid UTF-8: {error}') from error
    if any(not re.fullmatch(r'[^\s/]+/[^\s/]+', case) for case in required):
        raise ValueError(f'{directory.name}: invalid required case identifier')
    if len(set(required)) != len(required):
        raise ValueError(f'{directory.name}: repeated required case')
    if not results or not re.fullmatch(r'# casi: (0|[1-9][0-9]*)', results[0]):
        raise ValueError(f'{directory.name}: missing or invalid # casi: N header')
    if int(results[0][8:]) != len(results) - 1:
        raise ValueError(f'{directory.name}: # casi: N does not match result row count')
    identifiers = []
    for line in results[1:]:
        identifier, separator, value = line.partition(' ')
        if not separator or not value:
            raise ValueError(f'{directory.name}: invalid result row: {line!r}')
        identifiers.append(identifier)
    if len(set(identifiers)) != len(identifiers):
        raise ValueError(f'{directory.name}: repeated result case')
    if identifiers != required:
        raise ValueError(f'{directory.name}: result cases differ from obbligatori.txt (membership or order)')
    return required_bytes, result_bytes


def compare(directories):
    validated = [(directory, validate(directory)) for directory in directories]
    baseline, baseline_files = validated[0]
    for directory, files in validated[1:]:
        for name, expected, actual in zip(('obbligatori.txt', 'risultati.txt'), baseline_files, files):
            if expected != actual:
                diff = list(difflib.unified_diff(
                    expected.decode('utf-8').splitlines(keepends=True),
                    actual.decode('utf-8').splitlines(keepends=True),
                    fromfile=f'{baseline.name}/{name}', tofile=f'{directory.name}/{name}'))
                # repr also exposes CRLF and trailing-byte differences.
                details = '\n'.join(repr(line) for line in diff[:20])
                raise ValueError(f'{directory.name}: {name} differs byte for byte from {baseline.name}\n{details}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directories', type=Path, nargs=3)
    args = parser.parse_args()
    try:
        compare(args.directories)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1
    print('ADR 0008: all three systems have complete, byte-identical corpus results.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
