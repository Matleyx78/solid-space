# ADR 0006: Build, CI e sottomoduli

- **Stato:** Accettato
- **Data:** 2026-10-27
- **Corretto:** 2026-10-27 — le citazioni di tappe future per numero (contesto,
  punti 1 e 6, alternative) sono sostituite dall'evento che le fa nascere, perché
  le tappe del piano iniziale non esistono più
  ([ADR 0007](0007-tappe-su-richiesta.md)); lo decide il responsabile.
- **Corretto:** 2026-11-05 — i runner di Windows e macOS restano `windows-latest`
  e `macos-latest`, per scelta e non per dimenticanza; lo decide il responsabile.

## Contesto

L'[ADR 0002](0002-componenti-e-dipendenze.md) fissa i componenti, i target con alias
e l'esistenza di un'opzione per spegnere `kernel` e `features`; l'[ADR
0003](0003-convenzioni.md) fissa strumenti e avvisi. Restano da decidere i nomi
dell'opzione e dei preset, la matrice della CI, come si acquisisce GoogleTest e che
cosa fa la CI dei sottomoduli, finché OCCT non entra in gioco (con la tappa che lo porta).

Tecnigrafo ha già risolto problemi analoghi; qui si riprende il suo impianto dove
vale anche per una libreria.

## Decisione

1. **Opzione per spegnere il nucleo.** `SOLID_SPACE_BUILD_KERNEL`, attiva di
   default. Spenta, esclude `kernel` e `features`, e con loro OCCT. Dalla tappa che porta
   OCCT la CI prova sia con l'opzione accesa sia spenta.
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
   tappa che porta OCCT, OCCT si cerca come pacchetto di sistema o si compila con una cache.
7. **Un solo comando locale.** `scripts/controlla.sh` esegue gli stessi passi della
   CI, nello stesso ordine, e si ferma al primo errore.

## Alternative considerate

- **Avvisi come errori sempre accesi:** una libreria incorporata si romperebbe con
  compilatori che il progetto non ha provato.
- **Più compilatori su Linux fin dall'inizio** (GCC e Clang): più sicurezza, più
  tempo di CI; si riconsidera con la prima tappa che porta codice geometrico, quando c'è codice
  vero.
- **Scaricare i sottomoduli sempre:** OCCT è grande e non serve prima della tappa
  che lo porta.

## Conseguenze

- Le due opzioni e i preset sono parte dell'interfaccia di build: cambiarli è una
  decisione condivisa con Tecnigrafo, che incorporerà la libreria.
- I nomi dei controlli sono stabili: il ruleset di `main` li elenca per nome.
- **Runner:** Linux resta fissato a `ubuntu-24.04`; Windows e macOS restano
  `windows-latest` e `macos-latest`. Fissarli a `windows-2022` e `macos-15` è stato
  provato (pull request #199) e fa fallire la CI: il codice è scritto per le immagini
  attuali (Visual Studio 2026 dall'8–15 giugno 2026, macOS 26 da metà giugno 2026,
  secondo l'[annuncio di GitHub](https://github.blog/changelog/2026-05-14-github-actions-upcoming-image-migrations))
  e non compila sulle precedenti. Su `windows-2022` l'errore è C2220 (un avviso
  trattato come errore) in `tests/robust/tolerance_test.cpp`; su `macos-15` manca
  una `std::from_chars` adatta a `double` in `tests/corpus/esegui/plane_data.cpp`.
  Sono tutti due nei test, non nella libreria. Il rischio accettato è che
  un'ulteriore migrazione delle immagini renda la CI rossa senza che nessuno tocchi
  il codice: in quel caso si corregge il codice o si fissa l'immagine con una nuova
  correzione di questo ADR.
