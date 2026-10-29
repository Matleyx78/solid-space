# [0.2-P16] Banco delle prestazioni

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #…
- **Difficoltà:** media
- **Agente:** codex
- **Modello e impegno:** medio; impegno medio
- **Riferimenti:** non servono: il banco misura, non cambia il comportamento
- **Dipende da:** P11, P13, P15
- **Sblocca in Tecnigrafo:** niente
- **Componente:** nessuno (`tests/prestazioni/`)
- **Da leggere:** [ADR 0010](../../adr/0010-posizione-e-intersezioni-nel-piano.md)
  punto 8, [ADR 0011](../../adr/0011-raccordi-e-tangenze-nel-piano.md) punto 10,
  [ADR 0012](../../adr/0012-curva-parallela-nel-piano.md) punto 11;
  `geom2d/include/solid_space/geom2d/intersection.hpp`, `tangency.hpp`,
  `offset.hpp`; `tests/CMakeLists.txt`

## File

- `tests/prestazioni/banco.cpp` (nuovo)
- `tests/prestazioni/README.md` (nuovo)
- `tests/CMakeLists.txt`

## Cosa fare

Un programma, `solid_space_banco_prestazioni`, che misura gli obiettivi degli
ADR e stampa una tabella da copiare nelle note di rilascio. Non è un test della
CI (ADR 0010, punto 8): si compila con gli altri test, così resta compilabile,
ma `ctest` non lo esegue.

Misure, ciascuna su dati generati in modo deterministico (stesso seme, stessi
dati su ogni sistema) e con il tempo medio e il peggiore per chiamata:

- `intersect` su coppie non degeneri di ogni tipo, e su coppie quasi degeneri
  (quasi tangenti, quasi coincidenti), dove parte il calcolo esatto;
- decine di coppie insieme, come lo snap (obiettivo: sotto il millisecondo in
  tutto);
- `intersectAlong` di una curva contro 10, 100 e 1000 taglienti;
- `tangentsFrom` (lo snap a tangente);
- `offsetPolyline` su polilinee di 10, 100 e 500 pezzi, semplici.

Il README dice come compilarlo ed eseguirlo (preset `release`), che cosa misura
ogni riga e gli obiettivi degli ADR accanto a ciascuna.

## Regole della tappa che valgono qui

- Il banco usa solo le funzioni pubbliche, non le intestazioni interne.
- Nessuna nuova dipendenza: niente librerie di benchmark, solo `<chrono>`.
- Se una misura supera l'obiettivo dell'ADR, lo scrivi nella pull request: un
  miglioramento è una richiesta o un pacchetto nuovo, deciso dal responsabile.
- File comune: `tests/CMakeLists.txt` (solo le tue righe).

## Test richiesti

- Il banco si compila sui tre sistemi nella CI e gira in locale; la pull request
  riporta la tabella di una esecuzione, con il sistema e il processore.

## Fatto quando

- `solid_space_banco_prestazioni` esiste, misura le voci sopra e stampa la
  tabella con gli obiettivi.
- Il README del banco spiega come eseguirlo.
- La CI è verde.
