#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors
"""Exercise the comparison CLI with synthetic results, never corpus references."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class ComparisonTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directories = [Path(self.temporary.name) / name for name in ('linux', 'windows', 'macos')]
        for directory in self.directories:
            directory.mkdir()
            (directory / 'obbligatori.txt').write_bytes(b'robust/a\ngeom2d/b\n')
            (directory / 'risultati.txt').write_bytes(b'# casi: 2\nrobust/a 0x1p+0\ngeom2d/b true\n')

    def run_comparison(self):
        return subprocess.run(
            [sys.executable, '-B', str(Path(__file__).with_name('confronta_risultati.py')),
             *map(str, self.directories)], capture_output=True, text=True, check=False)

    def test_equal_and_empty(self):
        self.assertEqual(self.run_comparison().returncode, 0)
        for directory in self.directories:
            (directory / 'obbligatori.txt').write_bytes(b'')
            (directory / 'risultati.txt').write_bytes(b'# casi: 0\n')
        self.assertEqual(self.run_comparison().returncode, 0)

    def test_different_byte_reports_system_and_row(self):
        path = self.directories[1] / 'risultati.txt'
        path.write_bytes(path.read_bytes().replace(b'0x1p+0', b'0x1p+1'))
        result = self.run_comparison()
        self.assertEqual(result.returncode, 1)
        self.assertIn('windows', result.stderr)
        self.assertIn('robust/a 0x1p+1', result.stderr)

    def test_incomplete_or_malformed_results(self):
        invalid = {
            'missing case': b'# casi: 1\nrobust/a 1\n',
            'extra case': b'# casi: 3\nrobust/a 1\ngeom2d/b true\nrobust/c 2\n',
            'repeated case': b'# casi: 2\nrobust/a 1\nrobust/a 1\n',
            'wrong order': b'# casi: 2\ngeom2d/b true\nrobust/a 1\n',
            'wrong count': b'# casi: 3\nrobust/a 1\ngeom2d/b true\n',
            'missing header': b'robust/a 1\ngeom2d/b true\n',
            'invalid row': b'# casi: 2\nrobust/a\ngeom2d/b true\n',
            'invalid UTF-8': b'\xff',
        }
        for name, data in invalid.items():
            with self.subTest(name=name):
                (self.directories[2] / 'risultati.txt').write_bytes(data)
                result = self.run_comparison()
                self.assertEqual(result.returncode, 1)
                self.assertIn('macos', result.stderr)

    def test_missing_files(self):
        for name in ('risultati.txt', 'obbligatori.txt'):
            with self.subTest(name=name):
                path = self.directories[1] / name
                original = path.read_bytes()
                path.unlink()
                result = self.run_comparison()
                self.assertEqual(result.returncode, 1)
                self.assertIn('windows', result.stderr)
                self.assertIn(name, result.stderr)
                path.write_bytes(original)

    def test_different_required_lists(self):
        directory = self.directories[1]
        (directory / 'obbligatori.txt').write_bytes(b'robust/a\n')
        (directory / 'risultati.txt').write_bytes(b'# casi: 1\nrobust/a 0x1p+0\n')
        result = self.run_comparison()
        self.assertEqual(result.returncode, 1)
        self.assertIn('windows', result.stderr)
        self.assertIn('obbligatori.txt', result.stderr)

    def test_line_endings_are_compared_as_bytes(self):
        path = self.directories[1] / 'risultati.txt'
        path.write_bytes(path.read_bytes().replace(b'\n', b'\r\n'))
        self.assertEqual(self.run_comparison().returncode, 1)

    def test_duplicate_required_case(self):
        (self.directories[1] / 'obbligatori.txt').write_bytes(b'robust/a\nrobust/a\n')
        result = self.run_comparison()
        self.assertEqual(result.returncode, 1)
        self.assertIn('windows', result.stderr)


if __name__ == '__main__':
    unittest.main()
