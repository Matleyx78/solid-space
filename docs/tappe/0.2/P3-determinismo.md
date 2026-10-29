# [0.2-P3] Determinismo: opzioni e confronto tra i sistemi

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #…
- **Difficoltà:** media
- **Agente:** claude-2
- **Modello e impegno:** medio; impegno medio
- **Riferimenti:** non servono: opzioni di compilazione e CI, che non cambiano il
  comportamento della libreria
- **Dipende da:** P2
- **Sblocca in Tecnigrafo:** niente direttamente; serve a ogni pacchetto che
  calcola
- **Componente:** build e CI (più le righe dei target `robust` e `geom2d`)
- **Da leggere:** [ADR 0008](../../adr/0008-componente-geom2d.md) punto 5,
  [ADR 0006](../../adr/0006-build-ci-e-sottomoduli.md),
  `cmake/SolidSpaceWarnings.cmake`, `robust/CMakeLists.txt`,
  `geom2d/CMakeLists.txt`, `.github/workflows/ci.yml`, la sezione
  "Esecuzione" di `tests/corpus/README.md` (P2)

## File

- `cmake/SolidSpaceDeterminism.cmake` (nuovo)
- `CMakeLists.txt` (la riga che lo include)
- `robust/CMakeLists.txt`, `geom2d/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `scripts/confronta_risultati.py`, `scripts/test_confronta_risultati.py` (nuovi)
- `.github/workflows/ci.yml`

## Cosa fare

L'ADR 0008, punto 5, chiede che il primo pacchetto che realizza `robust` o
`geom2d` porti le opzioni e il job della CI. Questo pacchetto le porta prima di
qualunque calcolo, così ci sono già quando arriva il primo.

1. **`ss_configure_determinism(<target>)`** in `cmake/SolidSpaceDeterminism.cmake`,
   chiamata per `solid_space_robust` e `solid_space_geom2d`:
   - GCC e Clang: `-ffp-contract=off`; MSVC: `/fp:precise`, e nessun
     `/fp:contract`;
   - errore di configurazione se le opzioni del target o `CMAKE_CXX_FLAGS`
     contengono `-ffast-math`, `-Ofast`, `-funsafe-math-optimizations` o
     `/fp:fast` (punto 5.5), o se il sistema non è a 64 bit (punto 5.4), con un
     messaggio che cita l'ADR.
2. **Un controllo in `tests/CMakeLists.txt`** che fallisce la configurazione se uno
   dei due target non ha le opzioni del punto 1 (così un pacchetto successivo non
   le perde riscrivendo un `CMakeLists.txt`).
3. **Il confronto tra i sistemi.** Nei job Linux (GCC), Windows (MSVC) e macOS
   (Apple Clang, ARM64) i test girano con `SS_RISULTATI` (P2) che punta a un
   file, caricato come artefatto `risultati-<sistema>`. Un job nuovo,
   `Determinism (ADR 0008)`, aspetta i tre, scarica i file e li confronta con
   `scripts/confronta_risultati.py` (Python 3 con la sola libreria standard, come
   il validatore): uguali byte per byte, oppure errore con le
   prime righe diverse e il sistema da cui vengono. Con un archivio senza casi
   eseguiti i tre file sono vuoti e il confronto passa.
4. **Le regole sul codice** (calcolo solo nei `.cpp`, niente funzioni
   trigonometriche di `<cmath>`) non si controllano con uno strumento in questo
   pacchetto: le scrive P17 nei documenti, e le schede dei pacchetti le
   ripetono.

## Regole della tappa che valgono qui

- Non disattivare avvisi né controlli. `.clang-tidy` non cambia.
- File comuni: `robust/CMakeLists.txt`, `geom2d/CMakeLists.txt`,
  `tests/CMakeLists.txt` (solo le tue righe).
- Il job del determinismo segue le regole di `ci.yml`: gira solo quando gira la
  compilazione (`needs.references.outputs.build`).

## Test richiesti

- La configurazione con `-DCMAKE_CXX_FLAGS=-ffast-math` fallisce con il
  messaggio dell'ADR (provato a mano e scritto nella pull request).
- Il controllo del punto 2 fallisce se si toglie la chiamata da uno dei due
  target (provato a mano, scritto nella pull request).
- `test_confronta_risultati.py`, sotto `ctest`: file uguali passano; un byte
  diverso fallisce e mostra la riga e il sistema.
- Nella CI della pull request il job del determinismo gira e passa.

## Fatto quando

- `solid_space_robust` e `solid_space_geom2d` si compilano senza contrazione FMA
  sui tre compilatori, e la configurazione rifiuta le opzioni vietate.
- Il job `Determinism (ADR 0008)` confronta i risultati dei tre sistemi a ogni
  pull request che compila.
- La CI è verde.
