# ADR 0008: Il componente `geom2d`

- **Stato:** Accettato
- **Data:** 2026-10-29
- **Estende:** [ADR 0002](0002-componenti-e-dipendenze.md), senza sostituirlo

## Contesto

La tappa 0.7 di Tecnigrafo ha portato quattro richieste sulla geometria del
piano: posizione reciproca e intersezioni di linee, archi e cerchi (#20),
raccordi e tangenze (#21), curva parallela (#22) e una regola unica per
coincidenze e tolleranze (#23). Tutte e quattro hanno esito 3: serve un pezzo
nostro, perché `core` di Tecnigrafo compila senza OCCT (ADR 0030 di Tecnigrafo,
punto 5). Le valutazioni sono negli [ADR 0009](0009-regola-delle-tolleranze-del-piano.md)
e [ADR 0010](0010-posizione-e-intersezioni-nel-piano.md); quelle di #21 e #22
seguono.

L'[ADR 0002](0002-componenti-e-dipendenze.md) ha cinque componenti, e nessuno
contiene le curve del piano. `robust` vi è descritto come "predicati
orient/incircle, politica delle tolleranze": decide il segno di un'espressione,
non costruisce curve. Le due strade possibili:

- **A — allargare `robust`.** Nessun componente nuovo, e Tecnigrafo collega già
  `robust`. Ma `robust`, la base di `solver` e `kernel`, diventerebbe un
  contenitore misto di predicati e di algoritmi di costruzione (raccordi,
  offset), e ogni modifica all'offset toccherebbe il componente su cui poggia
  tutto. Si perderebbe il confine tra "decidere un segno" e "costruire una
  curva".
- **B — un componente nuovo sopra `robust`.** Strati puliti, un proprietario
  chiaro per ogni pacchetto, e le curve del piano riusabili dagli schizzi del
  `solver` e dai profili del `kernel`. Costa questo ADR, un target CMake e una
  riga nell'ADR 0030 di Tecnigrafo.

Il responsabile ha scelto B il 2026-10-29.

C'è poi un vincolo che riguarda entrambi i componenti: Tecnigrafo chiede lo
stesso risultato, bit per bit, con MSVC, GCC e Clang su Windows, Linux e macOS.
Le funzioni trigonometriche della libreria standard non lo danno (MSVC, glibc e
la libreria di macOS differiscono nell'ultimo bit), e la contrazione delle
moltiplicazioni e somme in FMA, che Clang fa per default dove l'hardware la
offre (per esempio sui Mac con processore ARM), cambia i risultati da una
piattaforma all'altra.

## Decisione

1. **Nuovo componente `geom2d`**: target CMake `solid_space_geom2d` con alias
   `solid_space::geom2d`, namespace `solid_space::geom2d`, cartella `geom2d/`,
   intestazioni pubbliche in `geom2d/include/solid_space/geom2d/`
   ([ADR 0003](0003-convenzioni.md), punto 4). Dipende solo dalla libreria
   standard e da `robust`, in modo pubblico perché le sue intestazioni usano i
   tipi di `robust`.
2. **Divisione dei compiti.**

   | Componente | Contiene |
   |---|---|
   | `robust` | i tipi di base (`Point2`, `Result`), i predicati esatti (decidere un segno), il calcolo deterministico (trigonometria propria), la politica delle tolleranze |
   | `geom2d` | le curve del piano (segmento, arco, cerchio, segmento di polilinea con bulge, polilinea), intersezioni e lato, tangenze e raccordi, curva parallela, e l'applicazione alle curve della regola delle tolleranze |

   `robust` non conosce le curve; `geom2d` non contiene soglie proprie: ogni
   tolleranza viene dalla politica di `robust`.
3. **Il grafo delle dipendenze diventa:**

   ```text
   features -> kernel, naming, solver
   kernel   -> robust, naming, geom2d (quando serve), OCCT (privato)
   solver   -> robust, geom2d (quando serve), libslvs (privato)
   geom2d   -> robust
   naming   -> (nessuna dipendenza)
   robust   -> (nessuna dipendenza)
   ```

   Oggi nessun componente collega `geom2d`: lo fa, se gli serve, la tappa che ne
   ha bisogno, senza un nuovo ADR perché la direzione è già ammessa qui.
4. **Si compila senza OCCT.** `geom2d` è costruito anche con
   `SOLID_SPACE_BUILD_KERNEL=OFF`. Il CAD 2D di Tecnigrafo collega `robust` e
   `geom2d`.
5. **Regola del determinismo**, per `robust` e `geom2d`:
   1. tutto il calcolo in virgola mobile sta nei file `.cpp` del componente, mai
      in funzioni `inline` o `constexpr` delle intestazioni pubbliche: quel
      codice verrebbe compilato con le opzioni di chi include;
   2. i due target si compilano senza contrazione FMA: `-ffp-contract=off` con
      GCC e Clang, `/fp:precise` senza `/fp:contract` con MSVC; una `std::fma`
      scritta esplicitamente resta ammessa, perché è arrotondata correttamente;
   3. di `<cmath>` si usano solo le funzioni che IEEE 754 arrotonda
      correttamente (`sqrt`, `fma`, `fabs` e simili); seno, coseno e arcotangente
      vengono dalla trigonometria propria di `robust`, che dà gli stessi bit
      ovunque;
   4. piattaforme a 64 bit con SSE2 (x86-64) o ARM64; niente x87, che calcola con
      una precisione intermedia diversa;
   5. chi incorpora la libreria non passa `-ffast-math` o `/fp:fast` ai target di
      solid-space.

   Il primo pacchetto che realizza `robust` o `geom2d` aggiunge le opzioni del
   punto 2 al target e un job della CI che confronta bit per bit i risultati dei
   tre sistemi operativi.
6. **Decisioni esatte, costruzioni con errore dichiarato, tolleranze solo per
   l'intenzione.** Ogni decisione di `robust` e `geom2d` (un segno, un ordine,
   un'intersezione, un lato) è esatta sui `double` ricevuti. Ogni costruzione
   (un punto, una lunghezza) dichiara nell'intestazione l'errore garantito. La
   regola delle tolleranze si applica solo con funzioni che la nominano, mai
   dentro una decisione (ADR 0009).
7. **Il tipo dei risultati.** `robust::Result<T, D>` porta un valore o una
   diagnosi, ed è la prima attuazione del punto 5 dell'ADR 0003. Ogni componente
   ha il suo tipo di diagnosi; `geom2d` usa `geom2d::Result<T>`, cioè
   `robust::Result<T, geom2d::Diagnosis>`. `naming`, che non dipende da
   `robust`, deciderà il suo tipo con la tappa che lo porta.
8. **Test e casi.** I test stanno in `tests/geom2d/` e i casi di verifica in
   `tests/corpus/casi/geom2d/`. Il primo pacchetto della tappa 0.2 aggiunge
   `geom2d` ai componenti dell'archivio (validatore e `tests/corpus/README.md`),
   prima dei pacchetti di riferimenti che vi scrivono.

## Alternative considerate

- **A, allargare `robust`**: vedi il contesto.
- **Le curve del piano in Tecnigrafo**: duplicherebbe i predicati esatti, che
  devono stare accanto alle costruzioni per essere coerenti con loro, e il
  `solver` di solid-space non potrebbe usarle per gli schizzi.
- **La parte 2D di OCCT** (`Geom2d`, `Geom2dGcc`): esclusa dall'ADR 0030 di
  Tecnigrafo, punto 5, e in ogni caso fuori dalla direzione delle dipendenze:
  OCCT lo collega solo `kernel` (ADR 0002, punto 5).
- **Un componente composto solo da intestazioni**: comodo da incorporare, ma il
  calcolo in virgola mobile verrebbe compilato con le opzioni di chi include, e
  il determinismo non sarebbe più nostro (punto 5).
- **Un `Result` per componente, senza tipo comune**: lo stesso codice ripetuto
  in ogni componente, con piccole differenze che chi lo usa deve ricordare.

## Conseguenze

- Il componente in più costa un target CMake, una cartella e una voce nei
  documenti che elencano i componenti: [ARCHITETTURA.md](../ARCHITETTURA.md),
  [README.md](../../README.md), [AGENTS.md](../../AGENTS.md) (direzione delle
  dipendenze), `.clang-tidy` e `scripts/controlla.sh`. Questa pull request li
  aggiorna; l'archivio dei casi lo aggiorna il primo pacchetto (punto 8).
- La regola del determinismo vincola anche `robust`, che oggi è vuoto: nessun
  codice esistente cambia.
- **Per Tecnigrafo** (lo decide il responsabile, con una pull request di
  Tecnigrafo): l'ADR 0030, punto 4, passa da "`core` usa solo
  `solid_space::robust`" a "`core` usa `solid_space::robust` e
  `solid_space::geom2d`". Il sottomodulo resta uno solo. Se Tecnigrafo compila
  con opzioni proprie, non deve dare `-ffast-math` o `/fp:fast` ai target di
  solid-space (punto 5.5).
- Le funzioni dichiarate nelle intestazioni del contratto non si usano finché il
  pacchetto che le realizza non è unito ([ADR 0007](0007-tappe-su-richiesta.md),
  punto 15).
