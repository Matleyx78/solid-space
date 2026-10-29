// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors
//
// Checks a `richiesta` issue (ADR 0007, point 3): the title, the six sections of
// .github/ISSUE_TEMPLATE/richiesta.yml and absolute issue references. It only
// reports problems; the workflow turns them into a comment and decides nothing.
// The messages are in Italian because they are posted on the issue.

'use strict';

const MARKER = '<!-- controlla-richiesta -->';

// Section labels of the issue form, in form order.
const SECTIONS = [
  'Da dove nasce',
  'Il bisogno',
  'Casi concreti',
  'Quando è soddisfatta',
  'Vincoli',
  'Che cosa blocca',
];

// What an issue form writes for an optional field left empty.
const NO_RESPONSE = /^_no response_$/i;

const TECNIGRAFO_REF =
  /(?:\bMatleyx78\/tecnigrafo#\d+|github\.com\/Matleyx78\/tecnigrafo\/(?:issues|pull|discussions)\/\d+)/i;

function normalize(text) {
  return (text || '').replace(/\r\n?/g, '\n');
}

// Removes the parts of Markdown where a `#N` is not a reference.
function stripNonReferences(text) {
  return text
    .replace(/<!--[\s\S]*?-->/g, ' ')
    .replace(/^(```|~~~)[^\n]*\n[\s\S]*?^\1[^\n]*$/gm, ' ')
    .replace(/`[^`\n]*`/g, ' ')
    .replace(/\bhttps?:\/\/\S+/g, ' ');
}

// Splits the body into sections keyed by their `##` or `###` heading.
function splitSections(body) {
  const sections = new Map();
  let current = null;
  let fence = null;
  for (const line of normalize(body).split('\n')) {
    const fenceMatch = /^(```|~~~)/.exec(line);
    if (fenceMatch) {
      if (fence === null) {
        fence = fenceMatch[1];
      } else if (fence === fenceMatch[1]) {
        fence = null;
      }
    }
    const heading = fence === null ? /^#{2,3}\s+(.+?)\s*#*\s*$/.exec(line) : null;
    if (heading) {
      current = heading[1].trim();
      if (!sections.has(current)) {
        sections.set(current, []);
      }
    } else if (current !== null) {
      sections.get(current).push(line);
    }
  }
  const result = new Map();
  for (const [name, lines] of sections) {
    result.set(name, lines.join('\n').trim());
  }
  return result;
}

function isEmpty(content) {
  const visible = content.replace(/<!--[\s\S]*?-->/g, '').trim();
  return visible === '' || NO_RESPONSE.test(visible);
}

// Returns the references that are not absolute, without duplicates.
function relativeReferences(text) {
  const cleaned = stripNonReferences(normalize(text));
  const found = new Set();
  const pattern = /(^|[^\w/.#-])((?:[\w.-]+)?#\d+)\b/g;
  let match;
  while ((match = pattern.exec(cleaned)) !== null) {
    found.add(match[2]);
  }
  return [...found];
}

// Returns the list of problems; an empty list means the request is complete.
function check(title, body) {
  const problems = [];
  const name = (title || '').trim();
  if (!/^Richiesta:\s*\S/.test(name)) {
    problems.push(
      'Il titolo deve cominciare con «Richiesta: » seguito dal bisogno in una frase.',
    );
  }

  const sections = splitSections(body);
  for (const section of SECTIONS) {
    if (!sections.has(section)) {
      problems.push(`Manca la sezione «${section}».`);
    } else if (isEmpty(sections.get(section))) {
      problems.push(`La sezione «${section}» è vuota.`);
    }
  }

  const origin = sections.get('Da dove nasce');
  if (origin !== undefined && !isEmpty(origin) && !TECNIGRAFO_REF.test(origin)) {
    problems.push(
      '«Da dove nasce» non cita la issue, la pull request o la discussione di Tecnigrafo: ' +
        'scrivila come `Matleyx78/tecnigrafo#N`.',
    );
  }

  const relative = relativeReferences(`${name}\n${normalize(body)}`);
  if (relative.length > 0) {
    const list = relative.map((ref) => `\`${ref}\``).join(', ');
    problems.push(
      `Riferimenti non assoluti: ${list}. Scrivili come \`Matleyx78/tecnigrafo#N\` ` +
        'o `Matleyx78/solid-space#N`: in una issue di solid-space un `#N` da solo ' +
        'punta a solid-space.',
    );
  }
  return problems;
}

function render(problems) {
  const header = `${MARKER}\n**Controllo automatico della richiesta** ([ADR 0007](https://github.com/Matleyx78/solid-space/blob/main/docs/adr/0007-tappe-su-richiesta.md), punto 3)\n\n`;
  if (problems.length === 0) {
    return `${header}La richiesta ora è completa. Il controllo non decide nulla: la valutazione la chiede il responsabile.\n`;
  }
  const items = problems.map((problem) => `- ${problem}`).join('\n');
  return (
    `${header}Da completare, modificando la issue:\n\n${items}\n\n` +
    'Le regole sono in [docs/richieste/0000-modello.md](https://github.com/Matleyx78/solid-space/blob/main/docs/richieste/0000-modello.md). ' +
    'Il controllo non decide nulla: la valutazione la chiede il responsabile.\n'
  );
}

module.exports = { MARKER, SECTIONS, check, render, relativeReferences, splitSections };
