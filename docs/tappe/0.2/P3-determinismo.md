# [0.2-P3] Determinismo e compilazione della tappa

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #…
- **Difficoltà:** media
- **Agente:** claude-2
- **Modello e impegno:** medio; impegno medio
- **Riferimenti:** non servono: opzioni di compilazione, raccolta dei sorgenti e
  CI, che non cambiano il comportamento della libreria
- **Dipende da:** P2
- **Sblocca in Tecnigrafo:** niente direttamente; serve a ogni pacchetto che
  calcola
- **Componente:** build e CI (più i target `robust` e `geom2d`)
- **Da leggere:** [ADR 0008](../../adr/0008-componente-geom2d.md) punto 5,
  [ADR 0006](../../adr/0006-build-ci-e-sottomoduli.md),
  `cmake/SolidSpaceWarnings.cmake`, `CMakeLists.txt`, `robust/CMakeLists.txt`,
  `geom2d/CMakeLists.txt`, `tests/CMakeLists.txt`, `.github/workflows/ci.yml`,
  `scripts/controlla.sh`, la sezione "Esecuzione" di `tests/corpus/README.md`
  (P2); la sezione 6 del [piano](../0.2-geometria-del-piano.md) («File comuni»)

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
qualunque calcolo, così ci sono già quando arriva il primo. E prepara la
compilazione in modo che i pacchetti di lavoro successivi non tocchino nessun
`CMakeLists.txt` (piano, sezione 6, «File comuni»).

1. **`ss_configure_determinism(<target>)`** in `cmake/SolidSpaceDeterminism.cmake`,
   chiamata per `solid_space_robust` e `solid_space_geom2d`:
   - GCC e Clang: `-ffp-contract=off`; MSVC: `/fp:precise`, e nessun
     `/fp:contract`;
   - errore di configurazione se le opzioni del target o `CMAKE_CXX_FLAGS`
     contengono `-ffast-math`, `-Ofast`, `-funsafe-math-optimizations` o
     `/fp:fast` (punto 5.5), o se il sistema non è a 64 bit (punto 5.4), con un
     messaggio che cita l'ADR.
2. **Un controllo in `tests/CMakeLists.txt`** che fallisce la configurazione se
   uno dei due target non ha le opzioni del punto 1.
3. **Raccolta dei sorgenti e dei test.** I file che la tappa aggiunge si
   raccolgono con `file(GLOB ... CONFIGURE_DEPENDS ...)`, che Ninja e Visual
   Studio ricontrollano a ogni compilazione:
   - `solid_space_robust`: `robust/src/*.cpp`; `solid_space_geom2d`:
     `geom2d/src/*.cpp` e `geom2d/src/exact/*.cpp`. I segnaposto attuali
     (`robust.cpp`, `geom2d.cpp`) restano finché P4 e P6 non li tolgono, quindi
     ogni target ha sempre almeno un sorgente;
   - due eseguibili di test di unità, `solid_space_robust_unit_test` e
     `solid_space_geom2d_unit_test`, con `tests/robust/*_test.cpp` e
     `tests/geom2d/*_test.cpp` tranne `contract_test.cpp` (che resta in
     `solid_space_contract_test`), con l'accesso alle intestazioni private di
     `robust/src/` e `geom2d/src/`, scoperti con `gtest_discover_tests`. Un
     eseguibile senza file si salta (oggi entrambi sono vuoti);
   - le operazioni dei casi (`tests/corpus/esegui/op_*.cpp`) le raccoglie già P2.

   Un commento in ogni `CMakeLists.txt` toccato dice perché si usa il glob qui:
   tenere i pacchetti paralleli della tappa senza file comuni.
4. **Il confronto tra i sistemi.** Il test `corpus_results` di P2 scrive, in un
   solo processo, `<build>/corpus/risultati.txt` con tutti e soli i casi
   obbligatori in ordine fisso, e il generatore scrive accanto
   `obbligatori.txt`. Nei job Linux (GCC), Windows (MSVC) e macOS (Apple Clang,
   ARM64), dopo i test, i due file si caricano come artefatto
   `risultati-<sistema>` (dalla cartella di build che `scripts/controlla.sh`
   usa in quel job). Un job nuovo, `Determinism (ADR 0008)`, aspetta i tre,
   scarica gli artefatti ed esegue `scripts/confronta_risultati.py` (Python 3
   con la sola libreria standard, come il validatore), che:
   - per ogni sistema controlla che il file esista, che la riga `# casi: N`
     corrisponda al numero di righe e che gli `<componente>/<id>` siano
     esattamente quelli di `obbligatori.txt`, nello stesso ordine e senza
     ripetizioni: tutti e soli i casi eseguiti;
   - controlla che i tre `obbligatori.txt` siano uguali (stessi casi compilati
     ovunque);
   - confronta i tre `risultati.txt` byte per byte, e in caso di differenza
     mostra le prime righe diverse con il sistema da cui vengono.

   Con un archivio senza casi obbligatori i file hanno la sola riga `# casi: 0`
   e il confronto passa.
5. **Le regole sul codice** (calcolo solo nei `.cpp`, niente funzioni
   trigonometriche di `<cmath>`) non si controllano con uno strumento in questo
   pacchetto: le scrive P18 nei documenti, e le schede dei pacchetti le
   ripetono.

## Regole della tappa che valgono qui

- Non disattivare avvisi né controlli. `.clang-tidy` non cambia.
- Dopo questo pacchetto, nella tappa, i `CMakeLists.txt` li toccano solo P17
  (il banco) e P18 (un commento), che dipendono da tutti gli altri: se un
  pacchetto di lavoro avesse bisogno di toccarli, la raccolta del punto 3 non
  basta, e va scritto nella issue.
- File comuni: `tests/CMakeLists.txt` (dopo P2, da cui P3 dipende);
  `.github/workflows/ci.yml` (dopo P1, da cui P2 dipende).
- Il job del determinismo segue le regole di `ci.yml`: gira solo quando gira la
  compilazione (`needs.references.outputs.build`).

## Test richiesti

- La configurazione con `-DCMAKE_CXX_FLAGS=-ffast-math` fallisce con il
  messaggio dell'ADR (provato a mano e scritto nella pull request).
- Il controllo del punto 2 fallisce se si toglie la chiamata da uno dei due
  target (provato a mano, scritto nella pull request).
- Un file `.cpp` aggiunto in `robust/src/` o un `*_test.cpp` in
  `tests/geom2d/` entra nella compilazione senza toccare i `CMakeLists.txt`
  (provato a mano con un file temporaneo, scritto nella pull request).
- `test_confronta_risultati.py`, sotto `ctest`: tre file uguali passano; un byte
  diverso fallisce e mostra la riga e il sistema; un caso mancante, uno in più,
  uno ripetuto, un ordine diverso da `obbligatori.txt`, un file mancante o un
  `# casi: N` sbagliato falliscono con un messaggio che dice quale sistema.
- Nella CI della pull request il job del determinismo gira e passa.

## Fatto quando

- `solid_space_robust` e `solid_space_geom2d` si compilano senza contrazione FMA
  sui tre compilatori, e la configurazione rifiuta le opzioni vietate.
- Sorgenti, test di unità e operazioni dei casi entrano nella compilazione senza
  toccare i `CMakeLists.txt`.
- Il job `Determinism (ADR 0008)` controlla la completezza dei risultati di ogni
  sistema e li confronta a ogni pull request che compila.
- La CI è verde.
