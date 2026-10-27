---
name: Riferimenti di un pacchetto (solo responsabile)
about: I casi di verifica di un pacchetto della tappa aperta, scritti prima del lavoro (ADR 0004). Li crea il responsabile all'apertura della tappa.
title: "[X.Y-RN] Riferimenti: "
labels: riferimenti
---

**Tappa:** [X.Y](../blob/main/docs/tappe/) — **Riferimenti del pacchetto:** PN (#…)
**Scheda del pacchetto:** `docs/tappe/X.Y/PN-<nome>.md`
**Dipende da:** #…

Scrivi i casi di verifica del pacchetto PN, prima che il lavoro cominci. Le regole
sono in [AGENTS.md](../blob/main/AGENTS.md), sezione "Robustezza e verifica", e
nell'[ADR 0004](../blob/main/docs/adr/0004-robustezza-e-verifica.md). Fa fede la
scheda del pacchetto PN.

## Cosa verificare

<!-- Le condizioni "Fatto quando" del pacchetto PN: ognuna deve avere almeno un
caso. Il responsabile può aggiungere qui casi o file che vuole vedere. -->

## In breve

- Tocca solo `tests/corpus/`.
- Valori attesi da calcoli a mano, aritmetica esatta o file pubblici, **mai**
  eseguendo solid-space.
- File presi dal web: indirizzo, versione, hash SHA-256 e licenza in
  `sorgenti.json`, link alla licenza nella pull request.
- Casi limite: valori nulli, punti coincidenti o quasi allineati, facce
  tangenti, spessori minimi.
