# [0.1-P5] Script per la CI

- **Tappa:** 0.1 — [piano](../0.1-fondamenta.md)
- **Issue:** [#7](https://github.com/Matleyx78/solid-space/issues/7)
- **Difficoltà:** media
- **Agente:** codex
- **Modello e impegno:** medio (sol medium); impegno medio
- **Riferimenti:** non servono: infrastruttura che non cambia il comportamento
- **Dipende da:** P3
- **Componente:** nessuno (`scripts/`)
- **Da leggere:** [ADR 0006](../../adr/0006-build-ci-e-sottomoduli.md),
  `.github/workflows/ci.yml` di P3, [AGENTS.md](../../../AGENTS.md) ("Quando
  fermarsi")

## File

- `scripts/ci-annota.sh`, `scripts/ci-attendi.sh`, `scripts/ci-errori.sh`
- `.github/workflows/ci.yml` (solo per collegare `ci-annota.sh` ai passi)
- `AGENTS.md` (solo la sezione "Quando fermarsi")

## Cosa fare

- Adattare a solid-space i tre script che Tecnigrafo usa allo stesso scopo
  (<https://github.com/Matleyx78/tecnigrafo>, stesso proprietario, licenza
  `GPL-3.0-or-later`; si scrive nel commit da quale commit vengono):
  - `ci-attendi.sh`: aspetta la CI dell'ultimo commit pubblicato con un solo
    comando che blocca. Esce 0 se tutto è verde, 1 se un controllo è fallito, 2 se
    l'API di GitHub non risponde, 3 se la CI è ancora in corso, 4 se è stata
    annullata, 5 dopo dieci attese di fila;
  - `ci-errori.sh`: per ogni job fallito, il passo e le poche righe di errore;
  - `ci-annota.sh`: trasforma gli errori di compilazione, test e formato in
    annotazioni leggibili, usato dai passi della CI.
- Usano solo l'API REST di GitHub (`gh api` o `curl` e `jq`): non la GraphQL.
- Aggiornare "Quando fermarsi" in `AGENTS.md`: sostituire la frase che rimanda al
  pacchetto P5 con l'uso dei due script.

## Regole della tappa che valgono qui

- Dipendenze solo da strumenti già usati: `bash`, `curl`, `jq`, `gh`.
- Non cambiare i nomi dei controlli della CI.
- Lo script deve funzionare senza token quando il repository è pubblico, oppure
  dirlo chiaramente.

## Test richiesti

- Prova su una pull request di prova: `ci-attendi.sh` esce 0 con la CI verde e 1
  con un controllo fallito volutamente.
- `ci-errori.sh` stampa il passo e le righe di errore del controllo fallito.
- Ogni script passa `shellcheck`.

## Fatto quando

- Un agente può aspettare la CI e leggerne gli errori con questi script, senza altri
  comandi.
- `AGENTS.md`, "Quando fermarsi", li indica.
