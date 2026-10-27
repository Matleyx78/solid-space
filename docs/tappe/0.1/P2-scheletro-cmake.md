# [0.1-P2] Scheletro CMake

- **Tappa:** 0.1 — [piano](../0.1-fondamenta.md)
- **Issue:** da creare all'apertura della tappa
- **Difficoltà:** media
- **Agente:** claude
- **Modello e impegno:** medio (Sonnet 5.5 alto); impegno medio
- **Riferimenti:** non servono: infrastruttura che non cambia il comportamento; il
  test del banco è il suo stesso riferimento
- **Dipende da:** ADR 0006 accettato
- **Componente:** tutti, solo come scheletro (più `tests/` e i file radice)
- **Da leggere:** [ADR 0002](../../adr/0002-componenti-e-dipendenze.md) (target,
  alias, opzione che spegne `kernel` e `features`), [ADR 0003](../../adr/0003-convenzioni.md)
  (strumenti, avvisi, intestazione SPDX), [ADR 0006](../../adr/0006-build-ci-e-sottomoduli.md)
  (nomi di opzioni e preset), [ARCHITETTURA.md](../../ARCHITETTURA.md) ("Dove si
  trova cosa")

## File

Circa 15, al limite della misura; se ne servono di più, il test del banco passa a
un pacchetto a parte.

- `CMakeLists.txt` (radice) e `CMakePresets.json`
- `cmake/SolidSpaceWarnings.cmake`
- per ciascuno dei cinque componenti (`robust`, `naming`, `solver`, `kernel`,
  `features`): `<componente>/CMakeLists.txt` e un sorgente `<componente>/src/<componente>.cpp`
- `tests/CMakeLists.txt` e il test del banco in `tests/banco/`

## Cosa fare

- **Un target per componente**, libreria statica, con alias `solid_space::<nome>`
  e namespace `solid_space::<nome>`. Dipendenze come nella tabella dell'ADR 0002:
  `solver` → `robust`; `kernel` → `robust`, `naming`; `features` → `kernel`,
  `naming`, `solver`. Nessuna dipendenza esterna: OCCT e libslvs non si collegano
  ancora.
- **Opzione `SOLID_SPACE_BUILD_KERNEL`** (attiva di default): spenta, non definisce
  `kernel` e `features`.
- **Opzione `SOLID_SPACE_WARNINGS_AS_ERRORS`** (spenta di default) e una funzione
  `ss_configure_target(<target>)` in `cmake/SolidSpaceWarnings.cmake`, che ogni
  target chiama.
- **Preset** `debug`, `release`, `asan` (AddressSanitizer e UBSan), tutti con gli
  avvisi come errori e con cartella `build$env{SS_BUILD_SUFFIX}/<preset>`. Il
  preset `tidy` lo aggiunge P3.
- **GoogleTest:** `find_package` e, se manca, `FetchContent` a versione fissata con
  hash.
- **Sorgenti dei componenti:** ognuno definisce almeno un simbolo, per evitare gli
  avvisi dell'archiviatore su macOS ("no symbols"). Nessun codice geometrico e
  nessuna nuova intestazione pubblica.
- **Test del banco:** un eseguibile che collega gli alias dei componenti presenti
  e prova che il banco funziona (per esempio che ogni target collega e che i
  componenti spenti non ci sono quando l'opzione è spenta).

## Regole della tappa che valgono qui

- Ogni file nuovo inizia con l'intestazione SPDX dell'ADR 0003 (anche i file CMake,
  con la sintassi dei commenti di CMake).
- `.clang-format` e `.clang-tidy` non esistono ancora: li porta P3, che riformatterà
  i file C++ di questo pacchetto.
- Non toccare `third_party/` né i documenti.

## Test richiesti

- Il test del banco, descritto sopra.
- Il test gira in configurazione `debug`, `release` e `asan`.

## Fatto quando

- `cmake --preset release` configura, compila e passa i test, senza avvisi.
- Ogni componente si compila da solo (`cmake --build --target solid_space_<nome>`
  o equivalente) e ha l'alias `solid_space::<nome>`.
- Con `-DSOLID_SPACE_BUILD_KERNEL=OFF` si configura e si compila senza OCCT e senza
  i target `kernel` e `features`.
- I preset `debug` e `asan` configurano e compilano.
- Nessuna intestazione pubblica nuova e nessuna dipendenza esterna oltre a
  GoogleTest.
