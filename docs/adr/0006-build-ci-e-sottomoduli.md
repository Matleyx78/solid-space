# ADR 0006: Build, CI e sottomoduli

- **Stato:** Proposto
- **Data:** 2026-10-27

## Contesto

L'[ADR 0002](0002-componenti-e-dipendenze.md) fissa i componenti, i target con alias
e l'esistenza di un'opzione per spegnere `kernel` e `features`; l'[ADR
0003](0003-convenzioni.md) fissa strumenti e avvisi. Restano da decidere i nomi
dell'opzione e dei preset, la matrice della CI, come si acquisisce GoogleTest e che
cosa fa la CI dei sottomoduli, finché OCCT non entra in gioco (tappa 0.5).

Tecnigrafo ha già risolto problemi analoghi; qui si riprende il suo impianto dove
vale anche per una libreria.

## Decisione

1. **Opzione per spegnere il nucleo.** `SOLID_SPACE_BUILD_KERNEL`, attiva di
   default. Spenta, esclude `kernel` e `features`, e con loro OCCT. Dalla tappa 0.5
   la CI prova sia con l'opzione accesa sia spenta.
2. **Avvisi come errori.** L'opzione `SOLID_SPACE_WARNINGS_AS_ERRORS` è **spenta di
   default**, perché chi incorpora la libreria con un altro compilatore non deve
   vederla fallire; la accendono tutti i preset del progetto. Gli avvisi sono quelli
   di Tecnigrafo (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion` e gli altri della
   sua configurazione; `/W4 /permissive-` su MSVC), applicati da una sola funzione
   CMake che ogni target del progetto chiama.
3. **Preset.** `debug`, `release`, `asan` (AddressSanitizer e UBSan) e `tidy`
   (clang-tidy), con cartella di build `build$env{SS_BUILD_SUFFIX}/<preset>`: più
   agenti sullo stesso PC non si pestano i piedi.
4. **GoogleTest.** Si cerca con `find_package`; se manca, si scarica con
   `FetchContent` a una versione fissata, verificata da un hash. La configurazione
   da zero richiede quindi la rete solo se il pacchetto di sistema manca.
5. **CI su GitHub Actions**, con questi controlli (i nomi servono al ruleset di
   `main`):
   - `Formatting (clang-format 18)`;
   - `Linux (GCC, clang-tidy)`, con il preset `release`;
   - `Windows (MSVC)` e `macOS (Apple Clang)`, con il preset `release`;
   - `Linux (ASan, UBSan)`, con il preset `asan`;
   - `Reference separation (ADR 0004)`, che rifiuta una pull request che mescola
     riferimenti e lavoro (ADR 0005, punto 7);
   - i controlli di compilazione si saltano se la pull request cambia solo
     documentazione.
6. **Sottomoduli.** La CI non scarica `third_party/occt` né
   `third_party/solvespace` finché un pacchetto non ne ha bisogno. Quando arriva la
   tappa 0.5, OCCT si cerca come pacchetto di sistema o si compila con una cache.
7. **Un solo comando locale.** `scripts/controlla.sh` esegue gli stessi passi della
   CI, nello stesso ordine, e si ferma al primo errore.

## Alternative considerate

- **Avvisi come errori sempre accesi:** una libreria incorporata si romperebbe con
  compilatori che il progetto non ha provato.
- **Più compilatori su Linux fin dall'inizio** (GCC e Clang): più sicurezza, più
  tempo di CI; si riconsidera dalla tappa 0.2, quando c'è codice vero.
- **Scaricare i sottomoduli sempre:** OCCT è grande e non serve prima della 0.5.

## Conseguenze

- Le due opzioni e i preset sono parte dell'interfaccia di build: cambiarli è una
  decisione condivisa con Tecnigrafo, che incorporerà la libreria.
- I nomi dei controlli sono stabili: il ruleset di `main` li elenca per nome.
- **Da confermare quando si scrive P3:** compilatori e versioni dei runner
  (si parte da `ubuntu-24.04`, `windows-latest`, `macos-latest`, come in Tecnigrafo
  per Linux).
