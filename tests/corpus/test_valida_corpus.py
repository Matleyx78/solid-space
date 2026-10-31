# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors
"""Tests of valida_corpus.py. The deliberately wrong files are in prove/; the
rest of the defects are built in memory from the sample case.

Run directly (python3 -B test_valida_corpus.py) or through ctest -L corpus.
"""

import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import valida_corpus as vc

HERE = Path(__file__).resolve().parent
PROVE = HERE / "prove"
SAMPLE = HERE / "casi" / "kernel" / "cubo-lato-1.json"
SCRIPT = HERE / "valida_corpus.py"
# Stand-in for a downloaded external file; its SHA-256 is computed, never stored.
PAYLOAD = b"solid-space verification corpus"
KEY = "sorgenti.dati-prova"


def sample():
    return json.loads(SAMPLE.read_text(encoding="utf-8"))


def check(data, **kwargs):
    kwargs.setdefault("component_dir", "kernel")
    return vc.check_case(data, "caso.json", "cubo-lato-1", **kwargs)


def good_sources():
    entry = {"nome": "dati-prova.bin", "indirizzo": "https://example.org/dati-prova.bin",
             "versione": "1.0", "sha256": hashlib.sha256(PAYLOAD).hexdigest(),
             "licenza": "CC0-1.0"}
    return {"versione": 1, "sorgenti": {"dati-prova": entry}}


def fields(problems):
    return {problem.field for problem in problems}


class CorpusTestCase(unittest.TestCase):
    def assertOnly(self, problems, file, field):
        """Exactly one problem, and its text names the file and the field."""
        self.assertEqual(len(problems), 1, [str(p) for p in problems])
        self.assertEqual((problems[0].file, problems[0].field), (file, field))
        self.assertIn(file, str(problems[0]))
        self.assertIn(field, str(problems[0]))

    def tmp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        return Path(directory.name)

    def corpus(self, case=None, where="kernel/x.json"):
        root = self.tmp()
        (root / "sorgenti.json").write_text('{"versione": 1, "sorgenti": {}}', encoding="utf-8")
        (root / "in-attesa").mkdir()
        (root / "casi").mkdir()
        if case is not None:
            path = root / "casi" / where
            path.parent.mkdir(parents=True)
            path.write_text(json.dumps(case), encoding="utf-8")
        return root


class AcceptedTest(CorpusTestCase):
    def test_real_corpus(self):
        self.assertEqual(vc.validate_corpus(HERE), [])

    def test_sample_case_alone(self):
        self.assertEqual(vc.validate_case_file(SAMPLE, expected_component="kernel"), [])

    def test_empty_sources(self):
        self.assertEqual(json.loads((HERE / "sorgenti.json").read_text())["sorgenti"], {})
        self.assertEqual(vc.validate_sources_file(HERE / "sorgenti.json"), [])

    def test_pending_line_for_existing_case(self):
        path = self.tmp() / "0.2-R4a.txt"
        path.write_text("# comment\n\nkernel/cubo-lato-1 #12\n", encoding="utf-8")
        self.assertEqual(vc.validate_pending_file(path, {"kernel/cubo-lato-1"}), [])

    def test_geom2d_case_with_nested_names_and_pending_block(self):
        case = dict(sample(), id="x", componente="geom2d",
                    atteso={"esito": "crossing", "punti": [
                        {"posizione": "atStart", "lato": "left", "esito": "done"}]})
        root = self.corpus(case, "geom2d/x.json")
        (root / "in-attesa" / "0.2-R4a.txt").write_text("geom2d/x #35\n", encoding="utf-8")
        self.assertEqual(vc.validate_corpus(root), [])

    def test_name_grammar(self):
        for value in ("a", "atStart", "aZ09", "crossing", "left", "done"):
            with self.subTest(value):
                self.assertEqual(check(dict(sample(), atteso={"nome": value})), [])

    def test_sources_with_matching_local_copy(self):
        root = self.tmp()
        (root / "esterni").mkdir()
        (root / "esterni" / "dati-prova.bin").write_bytes(PAYLOAD)
        (root / "s.json").write_text(json.dumps(good_sources()), encoding="utf-8")
        self.assertEqual(vc.validate_sources_file(root / "s.json"), [])


class RejectedFilesTest(CorpusTestCase):
    """The files in prove/: each has exactly one defect."""

    def test_missing_field(self):
        name = "caso-campo-mancante.json"
        problems = vc.validate_case_file(PROVE / name, expected_component="kernel")
        self.assertOnly(problems, name, "atteso")

    def test_missing_provenance(self):
        name = "caso-provenienza-assente.json"
        problems = vc.validate_case_file(PROVE / name, expected_component="kernel")
        self.assertOnly(problems, name, "provenienza")

    def test_bare_number(self):
        name = "caso-numero-senza-tolleranza.json"
        problems = vc.validate_case_file(PROVE / name, expected_component="kernel")
        self.assertOnly(problems, name, "atteso.volume")

    def test_incomplete_source(self):
        name = "sorgenti-voce-incompleta.json"
        data, problems = vc.load_json(PROVE / name, name)
        self.assertEqual(problems, [])
        self.assertOnly(vc.check_sources(data, name), name, f"{KEY}.licenza")

    def test_wrong_hash_of_local_copy(self):
        name = "sorgenti-hash-errato.json"
        root = self.tmp()
        shutil.copy(PROVE / name, root / name)
        self.assertOnly(vc.validate_sources_file(root / name), name, f"{KEY}.sha256")
        (root / "esterni").mkdir()
        (root / "esterni" / "dati-prova.bin").write_bytes(PAYLOAD)
        problems = vc.validate_sources_file(root / name)
        self.assertOnly(problems, name, f"{KEY}.sha256")
        self.assertIn(hashlib.sha256(PAYLOAD).hexdigest(), problems[0].message)

    def test_invalid_names(self):
        name = "caso-nome-non-valido.json"
        self.assertOnly(vc.validate_case_file(PROVE / name, expected_component="geom2d"),
                        name, "atteso.risultati[0].posizione")
        for value in ("At start", "_x", "1a", "a_b", "a-b", "à", "aé", ""):
            with self.subTest(value):
                self.assertOnly(check(dict(sample(), atteso={"nome": value})),
                                "caso.json", "atteso.nome")

    def test_external_copy_without_entry(self):
        problems = vc.validate_corpus(PROVE / "esterni-senza-voce")
        self.assertOnly(problems, "sorgenti.json", "sorgenti")
        self.assertIn("esterni/dati-prova.txt", problems[0].message)

    def test_source_without_copy(self):
        problems = vc.validate_corpus(PROVE / "voce-senza-copia")
        self.assertOnly(problems, "sorgenti.json", f"{KEY}.sha256")
        self.assertIn("esterni/dati-prova.bin", problems[0].message)

    def test_nested_external_copy_without_entry(self):
        root = self.corpus()
        (root / "esterni" / "nested").mkdir(parents=True)
        (root / "esterni" / "nested" / "x.bin").write_bytes(PAYLOAD)
        problems = vc.validate_corpus(root)
        self.assertOnly(problems, "sorgenti.json", "sorgenti")
        self.assertIn("esterni/nested/x.bin", problems[0].message)

    def test_pending_line_for_missing_case(self):
        name = "in-attesa-caso-inesistente.txt"
        problems = vc.validate_pending_file(PROVE / name, {"kernel/cubo-lato-1"})
        self.assertOnly(problems, name, "line 2")
        self.assertIn("kernel/non-esiste", problems[0].message)


def set_atteso(key, value):
    return lambda data: data["atteso"].update({key: value})


CASE_DEFECTS = [
    ("version 2", lambda d: d.update(versione=2), "versione"),
    ("version true", lambda d: d.update(versione=True), "versione"),
    ("id differs from file name", lambda d: d.update(id="altro"), "id"),
    ("id not lowercase words", lambda d: d.update(id="Cubo_1"), "id"),
    ("unknown component", lambda d: d.update(componente="occt"), "componente"),
    ("component differs from directory", lambda d: d.update(componente="robust"), "componente"),
    ("unknown field", lambda d: d.update(extra=1), "extra"),
    ("blank description", lambda d: d.update(descrizione="  "), "descrizione"),
    ("empty input", lambda d: d.update(ingresso={}), "ingresso"),
    ("null in input", lambda d: d["ingresso"].update(origine=None), "ingresso.origine"),
    ("bare number", set_atteso("volume", 1), "atteso.volume"),
    ("bare number in array", set_atteso("baricentro", ["1/2", 0.5, "1/2"]),
     "atteso.baricentro[1]"),
    ("zero denominator", set_atteso("volume", "1/0"), "atteso.volume"),
    ("decimal string", set_atteso("volume", "0.5"), "atteso.volume"),
    ("invalid name", set_atteso("volume", "At start"), "atteso.volume"),
    ("null value", set_atteso("valido", None), "atteso.valido"),
    ("zero tolerance", set_atteso("volume", {"valore": 1.0, "tolleranza": 0}),
     "atteso.volume.tolleranza"),
    ("negative tolerance", set_atteso("volume", {"valore": 1.0, "tolleranza": -1e-9}),
     "atteso.volume.tolleranza"),
    ("missing tolerance", set_atteso("volume", {"valore": 1.0}), "atteso.volume.tolleranza"),
    ("text tolerance", set_atteso("volume", {"valore": 1.0, "tolleranza": "1e-9"}),
     "atteso.volume.tolleranza"),
    ("infinite value", set_atteso("volume", {"valore": float("inf"), "tolleranza": 1.0}),
     "atteso.volume.valore"),
    ("extra key in approximate value",
     set_atteso("volume", {"valore": 1.0, "tolleranza": 1e-9, "x": 1}), "atteso.volume.x"),
    ("unknown provenance type", lambda d: d["provenienza"].update(tipo="a-occhio"),
     "provenienza.tipo"),
    ("missing procedure", lambda d: d["provenienza"].pop("procedimento"),
     "provenienza.procedimento"),
    ("blank procedure", lambda d: d["provenienza"].update(procedimento=""),
     "provenienza.procedimento"),
    ("public file without source", lambda d: d.update(provenienza={"tipo": "file-pubblico"}),
     "provenienza.sorgente"),
    ("source on a hand calculation", lambda d: d["provenienza"].update(sorgente="x"),
     "provenienza.sorgente"),
]

SOURCE_DEFECTS = [
    ("http address", {"indirizzo": "http://example.org/x"}, "indirizzo"),
    ("uppercase hash", {"sha256": "A" * 64}, "sha256"),
    ("short hash", {"sha256": "abc"}, "sha256"),
    ("unknown licence", {"licenza": "NOASSERTION"}, "licenza"),
    ("licence in prose", {"licenza": "free to use"}, "licenza"),
    ("path in file name", {"nome": "../x.bin"}, "nome"),
    ("blank version", {"versione": " "}, "versione"),
    ("extra field", {"extra": 1}, "extra"),
]


class DefectsTest(CorpusTestCase):
    def test_sample_is_clean(self):
        self.assertEqual(check(sample()), [])

    def test_case_defects(self):
        for label, mutate, field in CASE_DEFECTS:
            with self.subTest(label):
                data = sample()
                mutate(data)
                self.assertIn(field, fields(check(data)))

    def test_unknown_source_key(self):
        data = sample()
        data["provenienza"] = {"tipo": "file-pubblico", "sorgente": "non-esiste"}
        self.assertEqual(check(data, source_keys={"altra"})[0].field, "provenienza.sorgente")
        self.assertEqual(check(data, source_keys={"non-esiste"}), [])

    def test_source_defects(self):
        for label, change, field in SOURCE_DEFECTS:
            with self.subTest(label):
                data = good_sources()
                data["sorgenti"]["dati-prova"].update(change)
                self.assertIn(f"{KEY}.{field}", fields(vc.check_sources(data, "s.json")))

    def test_sources_with_repeated_file_name_or_bad_key(self):
        data = good_sources()
        data["sorgenti"]["Altra Voce"] = dict(data["sorgenti"]["dati-prova"])
        self.assertEqual(fields(vc.check_sources(data, "s.json")),
                         {"sorgenti.Altra Voce", "sorgenti.Altra Voce.nome"})

    def test_pending_lines(self):
        path = self.tmp() / "in-attesa.txt"
        path.write_text("kernel/a #1\nkernel/a #2\nsenza-numero\nocct/a #3\nkernel/a #0\n",
                        encoding="utf-8")
        problems = vc.validate_pending_file(path, {"kernel/a"})
        self.assertEqual(sorted(fields(problems)), ["line 2", "line 3", "line 4", "line 5"])


class JsonLoadingTest(CorpusTestCase):
    def load(self, text):
        path = self.tmp() / "x.json"
        path.write_text(text, encoding="utf-8")
        return vc.validate_case_file(path)

    def test_unreadable_json(self):
        for label, text in [("duplicate key", '{"a": 1, "a": 2}'), ("NaN", '{"a": NaN}'),
                            ("truncated", "{"), ("not an object", "[]")]:
            with self.subTest(label):
                self.assertEqual(fields(self.load(text)), {"(file)"})

    def test_missing_file(self):
        self.assertEqual(fields(vc.validate_case_file(self.tmp() / "no.json")), {"(file)"})


class CorpusLayoutTest(CorpusTestCase):
    def test_layout_defects(self):
        root = self.corpus(dict(sample(), id="x"))
        (root / "casi" / "kernel" / "notes.txt").write_text("x", encoding="utf-8")
        (root / "casi" / "occt").mkdir()
        (root / "casi" / "robust").mkdir()
        shutil.copy(root / "casi" / "kernel" / "x.json", root / "casi" / "robust" / "x.json")
        (root / "in-attesa").rmdir()
        problems = {(p.file, p.field) for p in vc.validate_corpus(root)}
        self.assertEqual(problems, {("casi/kernel/notes.txt", "(file)"),
                                    ("casi/occt", "(file)"),
                                    ("casi/robust/x.json", "componente"),
                                    ("in-attesa", "(file)")})

    def test_case_pointing_to_a_missing_source(self):
        case = dict(sample(), id="x", provenienza={"tipo": "file-pubblico", "sorgente": "manca"})
        problems = vc.validate_corpus(self.corpus(case))
        self.assertEqual([(p.file, p.field) for p in problems],
                         [("casi/kernel/x.json", "provenienza.sorgente")])

    def test_old_single_pending_list_is_rejected(self):
        root = self.corpus()
        (root / "in-attesa.txt").write_text("", encoding="utf-8")
        problems = vc.validate_corpus(root)
        self.assertOnly(problems, "in-attesa.txt", "(file)")
        self.assertIn("in-attesa/", problems[0].message)


class PendingDirectoryTest(CorpusTestCase):
    """The pending list is a directory with one file per block of references."""

    KNOWN = {"kernel/a", "kernel/b", "kernel/c"}

    def write(self, directory, name, text):
        (directory / name).write_text(text, encoding="utf-8")

    def test_blocks_are_read_together_and_dot_files_are_ignored(self):
        directory = self.tmp() / "in-attesa"
        directory.mkdir()
        self.write(directory, ".gitkeep", "")
        self.write(directory, "0.2-R4a.txt", "# comment\nkernel/a #12\n")
        self.write(directory, "0.2-R10b.txt", "kernel/b #13\nkernel/c #13\n")
        pending, problems = vc.load_pending_dir(directory, self.KNOWN)
        self.assertEqual(problems, [])
        self.assertEqual(pending, {"kernel/a": 12, "kernel/b": 13, "kernel/c": 13})

    def test_empty_directory_is_valid(self):
        directory = self.tmp() / "in-attesa"
        directory.mkdir()
        self.assertEqual(vc.load_pending_dir(directory, self.KNOWN), ({}, []))

    def test_file_name_must_be_the_tag_of_its_block(self):
        directory = self.tmp() / "in-attesa"
        directory.mkdir()
        for name in ("notes.txt", "0.2-r4a.txt", "R4a.txt", "0.2-R4.md", "0.2-R4aa.txt",
                     "0.2-R4a.txt.bak"):
            self.write(directory, name, "kernel/a #1\n")
        (directory / "0.2-R9.txt").mkdir()
        problems = vc.validate_pending_dir(directory, self.KNOWN)
        self.assertEqual(sorted(p.file for p in problems),
                         sorted(f"in-attesa/{n}" for n in (
                             "notes.txt", "0.2-r4a.txt", "R4a.txt", "0.2-R4.md",
                             "0.2-R4aa.txt", "0.2-R4a.txt.bak", "0.2-R9.txt")))
        self.assertEqual({p.field for p in problems}, {"(file)"})

    def test_file_names_with_and_without_a_letter_are_accepted(self):
        directory = self.tmp() / "in-attesa"
        directory.mkdir()
        for index, name in enumerate(("0.2-R9.txt", "0.2-R10a.txt", "1.0-R1c.txt")):
            self.write(directory, name, f"kernel/{'abc'[index]} #{index + 1}\n")
        self.assertEqual(vc.validate_pending_dir(directory, self.KNOWN), [])

    def test_file_without_cases_is_a_leftover(self):
        directory = self.tmp() / "in-attesa"
        directory.mkdir()
        self.write(directory, "0.2-R4a.txt", "# only a comment\n\n")
        problems = vc.validate_pending_dir(directory, self.KNOWN)
        self.assertOnly(problems, "in-attesa/0.2-R4a.txt", "(file)")
        self.assertIn("delete the file", problems[0].message)

    def test_case_listed_in_two_files(self):
        directory = self.tmp() / "in-attesa"
        directory.mkdir()
        self.write(directory, "0.2-R4a.txt", "kernel/a #1\n")
        self.write(directory, "0.2-R4b.txt", "kernel/b #2\nkernel/a #3\n")
        problems = vc.validate_pending_dir(directory, self.KNOWN)
        self.assertOnly(problems, "in-attesa/0.2-R4b.txt", "(file)")
        self.assertIn("in-attesa/0.2-R4a.txt", problems[0].message)

    def test_line_problems_name_the_file_and_the_line(self):
        directory = self.tmp() / "in-attesa"
        directory.mkdir()
        self.write(directory, "0.2-R4a.txt", "kernel/a #1\nkernel/non-esiste #2\n")
        problems = vc.validate_pending_dir(directory, self.KNOWN)
        self.assertOnly(problems, "in-attesa/0.2-R4a.txt", "line 2")
        self.assertIn("kernel/non-esiste", problems[0].message)

    def test_a_pending_file_in_the_corpus_must_point_to_an_existing_case(self):
        root = self.corpus(dict(sample(), id="x"))
        (root / "in-attesa" / "0.2-R4a.txt").write_text("kernel/y #1\n", encoding="utf-8")
        problems = vc.validate_corpus(root)
        self.assertOnly(problems, "in-attesa/0.2-R4a.txt", "line 1")


class CommandLineTest(CorpusTestCase):
    def run_script(self, *args):
        return subprocess.run([sys.executable, "-B", str(SCRIPT), *args],
                              capture_output=True, text=True, check=False)

    def test_valid_corpus_exits_zero(self):
        for root in (HERE, self.corpus(dict(sample(), id="x"))):
            result = self.run_script(str(root))
            self.assertEqual((result.returncode, result.stderr), (0, ""))
            self.assertIn("corpus valid", result.stdout)

    def test_invalid_corpus_exits_one_and_names_file_and_field(self):
        data = dict(sample(), id="x")
        del data["atteso"]
        result = self.run_script(str(self.corpus(data)))
        self.assertEqual(result.returncode, 1)
        self.assertIn("casi/kernel/x.json: atteso:", result.stderr)

    def test_not_a_directory_exits_two(self):
        self.assertEqual(self.run_script(str(SAMPLE)).returncode, 2)


if __name__ == "__main__":
    unittest.main()
