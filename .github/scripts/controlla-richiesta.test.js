// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors
//
// Tests for controlla-richiesta.js; run with
// `node --test .github/scripts/controlla-richiesta.test.js`.

'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const { MARKER, SECTIONS, check, render, relativeReferences } = require('./controlla-richiesta.js');

// Builds a body the way an issue form does: a `###` heading per field.
function formBody(overrides = {}) {
  const values = {
    'Da dove nasce': 'Tappa 0.7 di Tecnigrafo, dal piano in Matleyx78/tecnigrafo#330.',
    'Il bisogno': 'Sapere da che parte di un segmento sta un punto.',
    'Casi concreti': 'Un punto a 1e-13 dal segmento (0,0)-(1,0).',
    'Quando è soddisfatta': 'La risposta è la stessa sui tre sistemi operativi.',
    Vincoli: 'Solo C++20 e libreria standard.',
    'Che cosa blocca': 'Il pacchetto Matleyx78/tecnigrafo#331.',
    ...overrides,
  };
  return SECTIONS.filter((name) => values[name] !== null)
    .map((name) => `### ${name}\n\n${values[name]}`)
    .join('\n\n');
}

const TITLE = 'Richiesta: decidere da che parte sta un punto';

test('a complete request has no problems', () => {
  assert.deepEqual(check(TITLE, formBody()), []);
});

test('the title must start with "Richiesta: " and say something', () => {
  assert.equal(check('Richiesta: ', formBody()).length, 1);
  assert.equal(check('Punto e segmento', formBody()).length, 1);
});

test('a missing or empty section is reported by name', () => {
  const missing = check(TITLE, formBody({ Vincoli: null }));
  assert.deepEqual(missing, ['Manca la sezione «Vincoli».']);
  const empty = check(TITLE, formBody({ 'Casi concreti': '_No response_' }));
  assert.deepEqual(empty, ['La sezione «Casi concreti» è vuota.']);
  const comment = check(TITLE, formBody({ 'Il bisogno': '<!-- da scrivere -->' }));
  assert.deepEqual(comment, ['La sezione «Il bisogno» è vuota.']);
});

test('the origin must cite Tecnigrafo in absolute form or by URL', () => {
  const none = check(TITLE, formBody({ 'Da dove nasce': 'Tappa 0.7 di Tecnigrafo.' }));
  assert.equal(none.length, 1);
  assert.match(none[0], /Da dove nasce/);
  const url = formBody({
    'Da dove nasce': 'Tappa 0.7, https://github.com/Matleyx78/tecnigrafo/pull/330',
  });
  assert.deepEqual(check(TITLE, url), []);
});

test('bare and repository-only references are reported once each', () => {
  const body = formBody({
    'Che cosa blocca': 'Blocca #12, (#12), tecnigrafo#326 e #20–#23.',
  });
  const problems = check(TITLE, body);
  assert.equal(problems.length, 1);
  for (const ref of ['`#12`', '`tecnigrafo#326`', '`#20`', '`#23`']) {
    assert.ok(problems[0].includes(ref), ref);
  }
});

test('absolute references, code, comments and URLs are not references', () => {
  const text = [
    'Matleyx78/tecnigrafo#326 e Matleyx78/solid-space#20',
    '`#12` e <!-- #13 -->',
    'https://github.com/Matleyx78/tecnigrafo/issues/5#issuecomment-1',
    'colore #12ab34, titolo ## Sezione',
    '```',
    'riga #14',
    '```',
  ].join('\n');
  assert.deepEqual(relativeReferences(text), []);
});

test('Windows line endings and level-two headings are accepted', () => {
  const body = formBody().replace(/### /g, '## ').replace(/\n/g, '\r\n');
  assert.deepEqual(check(TITLE, body), []);
});

test('the comment carries the marker and lists the problems', () => {
  const problems = ['Manca la sezione «Vincoli».'];
  const body = render(problems);
  assert.ok(body.startsWith(MARKER));
  assert.ok(body.includes('- Manca la sezione «Vincoli».'));
  assert.ok(render([]).includes('completa'));
});
