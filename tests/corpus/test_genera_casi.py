#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from genera_casi import generate, value_cpp

EXECUTABLE = sys.argv.pop(1) if len(sys.argv) > 1 else None


class GeneratorTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / 'casi' / 'robust').mkdir(parents=True)
        (self.root / 'casi' / 'geom2d').mkdir()
        (self.root / 'sorgenti.json').write_text('{"versione": 1, "sorgenti": {}}')
        (self.root / 'in-attesa.txt').write_text('')
        self.output = self.root / 'generated'

    def case(self, name='example', component='robust', **changes):
        data = dict(versione=1, id=name, componente=component, descrizione='Fixture',
                    ingresso={'operazione': 'fixture', 'n': 0.5}, atteso={'n': '1/2'},
                    provenienza={'tipo': 'calcolo-a-mano', 'procedimento': 'One half.'})
        data.update(changes)
        path = self.root / 'casi' / component / (name + '.json')
        path.write_text(json.dumps(data), encoding='utf-8')
        return path

    def run_generator(self):
        generate(self.root, self.output, ['robust', 'geom2d'])
        return (self.output / 'casi.cpp').read_text(encoding='utf-8')

    def test_hex_and_json_numbers_are_identical(self):
        self.assertEqual(value_cpp(0.5), value_cpp('0x1p-1'))
        self.assertIn('-0x0.0p+0', value_cpp('-0x0p+0'))
        self.assertEqual(value_cpp(1), value_cpp('0x1p+0'))
        self.assertIn('Value::text', value_cpp('nan'))

    def test_exact_fraction_keeps_text_and_optional_double(self):
        self.assertEqual(value_cpp('1/2', True), 'Value::exact("1/2", 0x1.0000000000000p-1)')
        self.assertEqual(value_cpp('1/3', True), 'Value::exact("1/3", std::nullopt)')
        self.assertIn('std::nullopt', value_cpp(str(2**53 + 1), True))
        self.assertIn('std::nullopt', value_cpp(str(10**400), True))
        self.assertIn('std::nullopt', value_cpp('1/' + str(10**400), True))

    def test_pending_and_required_order_and_repeatability(self):
        self.case('z-last')
        self.case('b-pending')
        self.case('a-first', component='geom2d')
        (self.root / 'in-attesa.txt').write_text('# comment\nrobust/b-pending #35\n')
        first = self.run_generator()
        self.assertIn('"b-pending", "robust", 35', first)
        self.assertLess(first.index('"a-first"'), first.index('"z-last"'))
        self.assertEqual((self.output / 'obbligatori.txt').read_bytes(),
                         b'geom2d/a-first\nrobust/z-last\n')
        self.assertEqual(first, self.run_generator())
        self.assertLess(first.index('"operazione"'), first.index('"n"'))

    def test_one_function_per_case_and_table_function_without_case_data(self):
        for name in ('a-case', 'b-case', 'c-case'):
            self.case(name)
        code = self.run_generator()
        self.assertEqual(code.count('Case makeCase'), 3)
        self.assertEqual(code.count('&makeCase'), 3)
        self.assertLess(code.index('&makeCase0'), code.index('&makeCase1'))
        self.assertLess(code.index('&makeCase1'), code.index('&makeCase2'))
        # The function that builds the table holds no case data, so its stack frame
        # does not grow with the number of cases (Windows has 1 MiB by default).
        table = code[code.index('std::span<const Case> cases()'):]
        self.assertNotIn('Value::', table)
        self.assertIn('constexpr std::size_t count = 3;', table)

    def test_empty_archive_still_has_a_table(self):
        code = self.run_generator()
        self.assertNotIn('Case makeCase', code)
        self.assertIn('constexpr std::size_t count = 0;', code)
        self.assertIn('nullptr}', code)

    def test_invalid_case_uses_validator_diagnostic(self):
        self.case(atteso={'n': 1})
        with self.assertRaisesRegex(ValueError, 'atteso.n: bare number'):
            self.run_generator()
        self.assertFalse(self.output.exists())

    def test_unselected_component_and_empty_archive(self):
        (self.root / 'casi' / 'kernel').mkdir()
        self.case('cube', component='kernel')
        self.assertNotIn('"cube"', self.run_generator())
        self.assertEqual((self.output / 'obbligatori.txt').read_bytes(), b'')

    @unittest.skipUnless(EXECUTABLE, 'CMake supplies the real corpus executable')
    def test_real_program_arguments_and_empty_results(self):
        result_file = self.root / 'results.txt'
        command = [EXECUTABLE, '--ss-results=' + str(result_file)]
        self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
        lines = result_file.read_bytes().splitlines()
        self.assertEqual(lines[0], f'# casi: {len(lines) - 1}'.encode())
        self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
        self.assertEqual(subprocess.run(command + ['--gtest_filter=*'],
                                        capture_output=True).returncode, 2)
        result_file.unlink()
        subprocess.run([EXECUTABLE], check=True, capture_output=True, cwd=self.root)
        self.assertFalse(result_file.exists())


if __name__ == '__main__':
    unittest.main()
