# [0.1-P3] Controlli automatici

- **Tappa:** 0.1 — [piano](../0.1-fondamenta.md)
- **Issue:** [#5](https://github.com/Matleyx78/solid-space/issues/5)
- **Difficoltà:** media
- **Agente:** codex
- **Modello e impegno:** medio (sol medium); impegno medio
- **Riferimenti:** non servono: infrastruttura che non cambia il comportamento; la
  CI della sua stessa pull request è la prova
- **Dipende da:** P2, ADR 0005 (percorsi dell'archivio), ADR 0006
- **Componente:** nessuno (file radice, `scripts/`, `.github/workflows/`)
- **Da leggere:** [ADR 0003](../../adr/0003-convenzioni.md), [ADR
  0004](../../adr/0004-robustezza-e-verifica.md) (separazione riferimenti e lavoro),
  [ADR 0005](../../adr/0005-formato-dei-casi-di-verifica.md) (percorsi),
  [ADR 0006](../../adr/0006-build-ci-e-sottomoduli.md) (controlli e preset),
  [AMBIENTE-SVILUPPO.md](../../AMBIENTE-SVILUPPO.md)

## File

- `.clang-format` e `.clang-tidy`
- `.github/workflows/ci.yml`
- `scripts/controlla.sh`
- `AGENTS.md` (solo la sezione "Comandi")
- i file C++ di P2, riformattati
- `CMakePresets.json` (solo la riga del preset `tidy`)

## Cosa fare

- **`.clang-format` e `.clang-tidy`: stessa impostazione di Tecnigrafo.** Si
  copiano da <https://github.com/Matleyx78/tecnigrafo> (stesso proprietario,
  licenza `GPL-3.0-or-later`): è l'eccezione esplicita alla regola sul codice altrui
  che la scheda di tappa e l'ADR 0003 prevedono. Nel messaggio del commit si scrive
  da quale commit di Tecnigrafo vengono. Si tolgono solo le regole che citano
  moduli di Tecnigrafo che qui non esistono.
- **CI** (`ci.yml`) con i controlli dell'ADR 0006, ciascuno col nome scritto lì:
  `Formatting (clang-format 18)`, `Linux (GCC, clang-tidy)`, `Windows (MSVC)`,
  `macOS (Apple Clang)`, `Linux (ASan, UBSan)`, `Reference separation (ADR 0004)`.
  I controlli di compilazione si saltano per pull request di sola documentazione.
  La CI non scarica i sottomoduli.
- **Separazione riferimenti/lavoro:** una pull request che cambia
  `tests/corpus/casi/` o `tests/corpus/sorgenti.json` non cambia altri file, salvo
  `tests/corpus/in-attesa.txt`.
- **`scripts/controlla.sh`**: un solo comando locale con gli stessi passi della CI
  (formato, configura e compila, test), che si ferma al primo errore e stampa la
  coda del log. Opzioni: `--tidy` (clang-tidy sui file toccati), `--solo <passo>`.
  Usa `SS_BUILD_SUFFIX` per la cartella di build.
- **`AGENTS.md`, "Comandi":** sostituire il paragrafo "non esistono ancora comandi"
  con i comandi veri, lasciando intatto il resto della sezione.

## Regole della tappa che valgono qui

- Si può modificare `.github/` e i file di formattazione in questo pacchetto (di
  norma sono vietati agli agenti).
- Nessuna dipendenza nuova. Nessun avviso del compilatore disattivato per far
  passare la CI.
- Gli script per attendere la CI e leggerne gli errori sono di P5, non di questo
  pacchetto.

## Test richiesti

- La CI della pull request è verde su tutti e sei i controlli, anche su Windows e
  macOS.
- `scripts/controlla.sh` termina con successo da un clone pulito su Linux.
- Una prova di formattazione sbagliata volutamente (in un commit di prova da
  scartare) fa fallire `Formatting`: si riferisce nella pull request.

## Fatto quando

- La formattazione e l'analisi statica girano in CI e in locale con un solo comando.
- Il job con sanitizer è verde su Linux.
- `cmake --preset release` passa su Linux, Windows e macOS in CI, senza avvisi.
- Il controllo `Reference separation` rifiuta una pull request che mescola
  riferimenti e altri file e accetta una che cambia solo `in-attesa.txt`.
- `AGENTS.md`, "Comandi", descrive il comando locale.
