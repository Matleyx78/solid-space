# ADR 0016: Tangente, arcocoseno e lunghezza di un vettore uguali su tutti i sistemi

- **Stato:** Proposto
- **Data:** 2026-11-11
- **Richiesta:** #216 — da Tecnigrafo: tappa 0.8, Matleyx78/tecnigrafo#661
  (decisione in Matleyx78/tecnigrafo#620)
- **Esito:** 3 pezzo nostro
- **Tappa:** 0.3, per Tecnigrafo 0.8 (scheda da scrivere all'apertura)

## Richiesta

Il confronto tra sistemi della CI di Tecnigrafo vuole i rapporti dei casi di
verifica identici su Linux, Windows e macOS, e oggi ha 17 eccezioni: le ultime
cifre di alcuni numeri cambiano perché le funzioni di `<cmath>` non danno gli
stessi bit dappertutto. Tecnigrafo passa a `robust::sinCos` e `robust::atan2`,
che ci sono già; gli mancano la tangente (bulge degli archi delle polilinee,
testo obliquo), l'arcocoseno (passo di suddivisione degli archi) e la lunghezza
di un vettore `hypot(x, y)` (`Vec2` e distanza fra due punti). Le vuole uguali
bit per bit su tutti i sistemi, con un errore dichiarato, senza librerie
esterne e senza OCCT. Il testo completo è nella issue #216.

## Valutazione

- **Spetta a solid-space.** Il calcolo deterministico, compresa la
  trigonometria propria, è compito di `robust`
  ([ADR 0008](0008-componente-geom2d.md), punto 2 e punto 5.3: di `<cmath>` si
  usano solo le funzioni che IEEE 754 arrotonda correttamente, il resto viene
  da `robust`).
- **Esiti 1 e 2 esclusi da un vincolo già preso:** `core` di Tecnigrafo compila
  senza OCCT (ADR 0030 di Tecnigrafo, punto 5;
  [ADR 0007](0007-tappe-su-richiesta.md), punto 8). La richiesta lo ripete nei
  vincoli.
- **Esito 3**, in `robust`, accanto a `sinCos` e `atan2`
  (`trigonometry.hpp`). Nessun componente nuovo e nessuna dipendenza nuova.
- **Su che cosa si basa:**
  - la realizzazione di `sinCos` e `atan2` in `robust/src/trigonometry.cpp`
    (tappa 0.2, P5), con la sua analisi dell'errore: riduzione dell'angolo in
    cinque parti di π/2, aritmetica double-double, polinomi di Taylor, errore
    entro 1,002 ulp. La tangente e l'arcocoseno si costruiscono con gli stessi
    pezzi: la tangente come quoziente in double-double di seno e coseno dello
    stesso angolo ridotto, l'arcocoseno come arcotangente di
    √((1 − x)(1 + x)) / x, con l'arcotangente già scritta;
  - la ricerca esaustiva riportata in quel file (passo 1 di `sinCos`): ogni
    `double` fino a 2^20 dista almeno 2^-60,49 da ogni multiplo non nullo di
    π/2. Quindi nessun `double` è un polo della tangente e il modulo del
    risultato resta sotto 2^61;
  - gli standard C e C++, che non chiedono a `tan`, `acos` e `hypot` di
    arrotondare correttamente: le tre librerie possono dare ultimi bit diversi,
    come per seno, coseno e arcotangente (ADR 0008, contesto);
  - il codice di Tecnigrafo letto (solo lettura): `std::tan` in sei punti
    (bulge da un'ampiezza, `tan(sweep / 4)` e `tan(alpha / 2)`; angolo di
    obliquità del testo), `std::acos` in uno (`2 · acos(1 − tol / r)`),
    `std::hypot` in due (`Vec2::length` e la distanza in `command_runner`).
- **Non verificato:** nessun prototipo è stato eseguito. Che le 17 eccezioni
  dipendano solo da queste funzioni e da `sinCos` e `atan2` non si è potuto
  controllare: i rapporti dei tre sistemi stanno negli artefatti della CI di
  Tecnigrafo, che da qui non si leggono. Lo dice il criterio di chiusura.

### Che cosa garantisce il contratto

Come per `sinCos` e `atan2`, il contratto promette gli stessi bit ovunque e un
errore massimo, non l'arrotondamento corretto (la richiesta non lo chiede).
L'errore è **2 ulp** (`kElementaryMaxErrorUlps`), lo stesso margine di
`kTrigMaxErrorUlps`: la realizzazione di `sinCos` sta sotto 1,002 ulp, e il
margine lascia libero chi realizza senza toccare Tecnigrafo, i cui rapporti
confrontano gli stessi bit fra i sistemi, non con un valore esatto.

Oltre all'errore, alcune promesse **per struttura**, scelte perché Tecnigrafo
le vede o ci costruisce sopra:

- **tangente:** sempre finita (sotto 2^61), dispari bit per bit, ±0 per ±0;
- **arcocoseno:** dentro [+0, π arrotondato], 1 dà +0 e −1 dà π arrotondato,
  esattamente (come `atan2`, che dà il `double` più vicino a π);
- **lunghezza:** esatta quando la lunghezza esatta è un `double` (3 e 4 danno
  5, (x, 0) dà |x|), mai sotto la componente maggiore (così a / hypot(a, b)
  sta in [−1, 1] e si può dare all'arcocoseno), indipendente da segni e ordine
  degli argomenti, senza traboccamenti intermedi, subnormali compresi.

La lunghezza esatta quando è un `double` costa poco: la somma dei quadrati in
double-double (due prodotti esatti con `fma` e una somma esatta) e una
correzione della radice. Senza questa promessa una lunghezza intera come 5
potrebbe uscire 5,000000000000001 nei rapporti.

### I casi della richiesta

| Caso | Risposta del contratto |
|---|---|
| Bulge di un arco da 90°: `tan(π/8)` | entro 2 ulp di √2 − 1, con l'angolo dato dal `double` di π/8 |
| Bulge di un arco quasi intero: tangente vicino a π/2 | finita (sotto 2^61), entro 2 ulp della tangente del `double` dato; quanto il risultato cambi con l'argomento dipende da Tecnigrafo, che calcola l'angolo |
| Arcocoseno vicino a 1 e a −1 | entro 2 ulp; 1 dà +0 e −1 dà π arrotondato, esatti; vicino a 1, dove l'arcocoseno è circa √(2(1 − x)), l'errore resta relativo |
| `hypot(1e-12, 1e6)` | entro 2 ulp di 1e6, mai sotto 1e6 |
| `hypot` di componenti uguali | entro 2 ulp di a·√2 |
| `hypot(0, 0)` | +0, esatto |
| I 17 casi di `docs/verifica/sistemi.md` di Tecnigrafo | gli stessi bit sui tre sistemi per ogni funzione chiamata; il criterio si verifica dal lato di Tecnigrafo |

## Alternative considerate

- **Esito 1 o 2 (OCCT):** esclusi dall'ADR 0030 di Tecnigrafo, punto 5; OCCT
  stesso usa `<cmath>`.
- **Esito 4, le funzioni scritte in `core` di Tecnigrafo:** l'ADR 0008 (punto
  5.3) mette la trigonometria deterministica in `robust`; `core` dovrebbe
  rifare la riduzione e l'aritmetica double-double che `robust` ha già, e il
  calcolo deterministico starebbe in due posti.
- **Una libreria esistente.** CORE-MATH (licenza MIT) dà funzioni arrotondate
  correttamente, quindi uguali ovunque; ma copiare codice chiede una decisione
  del responsabile con fonte e licenza (ADR 0003 di Tecnigrafo,
  [AGENTS.md](../../AGENTS.md)), porta tabelle e codice per ogni funzione, e
  dà l'arrotondamento corretto che non serve. `robust` ha già i pezzi.
- **`hypot` come radice di x·x + y·y in `robust`:** deterministica e semplice,
  ma i quadrati traboccano oltre circa 2^512 e perdono precisione sotto 2^-511, e non garantisce
  la lunghezza esatta quando è un `double`. La scala per una potenza di 2 e la
  somma in double-double tolgono i due difetti con poche righe.
- **Aggiungere le tre funzioni a `trigonometry.hpp`:** il posto più naturale,
  ma cambierebbe un'intestazione già accettata, con la revisione delle
  correzioni prima del merge ([CONTRATTI.md](../CONTRATTI.md)). Un'intestazione
  nuova, `elementary.hpp`, lascia intatto il contratto di `sinCos` e `atan2` e
  ne usa il limite `kMaxTrigArgument`.
- **Arcocoseno che riporta in [−1, 1] i valori fuori:** nasconderebbe un errore
  del chiamante (un coseno calcolato male) invece di segnalarlo. Le funzioni di
  `robust` hanno precondizioni controllate in debug, e la promessa di `hypot`
  (mai sotto la componente maggiore) dà a Tecnigrafo un modo sicuro di
  calcolare un coseno nell'intervallo.
- **Anche arcoseno e arcotangente di un solo argomento:** non chiesti
  (ADR 0007, punto 2). L'arcotangente `atan(b)` che Tecnigrafo usa per il bulge
  è `atan2(b, 1)`, che c'è già.

## Decisione

1. **Tre funzioni nuove in `robust`**, dichiarate in
   `robust/include/solid_space/robust/elementary.hpp`: `tan(angle)`,
   `acos(x)` e `hypot(x, y)`, tutte `noexcept`, senza diagnosi, con
   precondizioni controllate in debug, come `sinCos` e `atan2`.
2. **Determinismo:** la regola dell'ADR 0008, punto 5. Il calcolo sta nei
   sorgenti di `robust`, con le opzioni di virgola mobile già in uso e solo
   operazioni che IEEE 754 arrotonda correttamente (`+`, `−`, `×`, `/`,
   `sqrt`, `fma`, `round`, `fabs`, `copysign` e simili). Stessi dati, stessi
   bit sui tre sistemi.
3. **Errore:** ogni risultato sta entro `kElementaryMaxErrorUlps` = 2 ulp del
   valore esatto della funzione nei `double` dati (per un risultato
   subnormale l'unità è 2^-1074). Il commento della realizzazione ne porta
   l'analisi, come quello di `trigonometry.cpp`.
4. **Domini:** la tangente accetta gli angoli fino a `kMaxTrigArgument`
   (2^20), il limite di `sinCos`; l'arcocoseno [−1, 1]; la lunghezza
   componenti fino a `kMaxHypotArgument` (2^1023), così il risultato è sempre
   finito.
5. **Promesse per struttura**, scritte nell'intestazione: quelle della sezione
   «Che cosa garantisce il contratto».
6. **Realizzazione** in `robust/src`, riusando la riduzione, le costanti e
   l'aritmetica double-double di `trigonometry.cpp`; le funzioni di supporto
   possono passare in un'intestazione privata di `robust/src`, senza cambiare
   `sinCos` e `atan2` né i loro bit.

## Contratto

Intestazione pubblica in bozza in questa pull request (ADR 0007, punto 15):

- `robust/include/solid_space/robust/elementary.hpp`: le costanti
  `kElementaryMaxErrorUlps` (2 ulp, l'errore garantito delle tre funzioni) e
  `kMaxHypotArgument` (2^1023), e le funzioni `tan`, `acos` e `hypot`, con
  precondizioni, intervalli dei risultati, valori esatti e simmetrie.

Nessuna diagnosi: come `sinCos` e `atan2`, le tre funzioni hanno solo
precondizioni. Nessuna intestazione già accettata cambia
(`trigonometry.hpp` si include per `kMaxTrigArgument`). Le funzioni sono solo
dichiarate; `tests/robust/contract_test.cpp` controlla che l'intestazione
compili con le firme e le costanti dichiarate.

## Conseguenze

- Componente: `robust`, con un'intestazione nuova; nessuna dipendenza nuova.
- **Per Tecnigrafo** (lo decide il responsabile; da qui non si scrive in
  Tecnigrafo):
  - `std::tan`, `std::acos` e `std::hypot` di `core` e `cmd` passano a
    `robust::tan`, `robust::acos` e `robust::hypot` (PT41), dopo che
    `std::sin`, `std::cos` e `std::atan2` sono passate a `sinCos` e `atan2`
    (PT1);
  - `std::atan(bulge)` (in `core/src/polyline.cpp` e
    `cmd/src/commands/pline_command.cpp`) non è nella richiesta ma sta nello
    stesso calcolo degli archi: `robust::atan2(bulge, 1.0)` è la stessa
    funzione, senza richieste nuove;
  - `render/src/scene_renderer.cpp` usa `std::pow` e `std::log10` per il passo
    della griglia: non sono trigonometria e la richiesta non le copre; se un
    giorno entrano in un rapporto, servono una richiesta o un calcolo diverso;
  - `std::fmod` e `std::remainder` sono esatte in IEEE 754 e possono restare;
  - una funzione dichiarata non si usa finché il suo pacchetto non è unito
    (ADR 0007, punto 15): PT41 aspetta i pacchetti di questa richiesta, non
    l'inizio della tappa.
- **Rischi.** Se dopo il passaggio qualche caso resta diverso fra i sistemi, la
  causa è altrove (una funzione non elencata, la formattazione dei numeri): lo
  si vede dal criterio di chiusura, e la correzione è di Tecnigrafo o una
  richiesta nuova. La tangente vicino ai poli è la parte delicata della
  verifica: i riferimenti chiedono valori calcolati con molte cifre.
- **Pacchetti, in via indicativa** (il piano lo scrive il responsabile
  all'apertura), dopo la revisione del contratto `[0.3-CS]`, ognuno con i suoi
  riferimenti:
  - `hypot`, media;
  - `tan` e `acos`, impegnativa, sopra la riduzione e l'arcotangente di
    `trigonometry.cpp`.
- Fuori da questa richiesta: arcoseno, arcotangente di un argomento,
  esponenziali e logaritmi.

## Verifica

- **Casi di riferimento** in `tests/corpus/casi/robust/`, scritti prima da un
  agente diverso da chi scrive il codice (ADR 0004), come quelli di `sinCos` e
  `atan2`:
  - `tan`: i multipli di π/8 e π/4, l'angolo di π/8 dei bulge da 90°, angoli
    vicini a π/2 (un quarto di un'ampiezza vicina a 2π) e vicini ai poli
    lontani fino a 2^20, il `double` più vicino a un polo trovato dalla
    ricerca esaustiva, ±0, angoli sotto 2^-27, gli angoli di obliquità del
    testo;
  - `acos`: ±1, 0, ±1/2, 1 − 2^-53 e −1 + 2^-53, valori come 1 − tol / r del
    passo degli archi, numeri sotto 2^-27;
  - `hypot`: le terne pitagoriche (3, 4, 5), (5, 12, 13), anche scalate per
    potenze di 2 fino ai subnormali, (x, 0), (0, 0), (1e-12, 1e6), componenti
    uguali, componenti vicine a `kMaxHypotArgument` e subnormali.
- **Valori attesi** indipendenti dal programma: serie calcolate con il modulo
  `decimal` di Python a molte cifre, come per `sinCos` e `atan2`, confrontate
  con la tolleranza dell'errore garantito (ADR 0013, punto 5); esatti i valori
  che il contratto fissa per struttura (le lunghezze esatte, gli estremi
  dell'arcocoseno, gli zeri).
- **Proprietà**, provate dai test del pacchetto su dati generati in modo
  deterministico: `tan` dispari bit per bit e sempre finita; `acos` dentro
  [+0, π arrotondato]; `hypot` uguale per segni e ordine degli argomenti, mai
  sotto la componente maggiore, esatta sulle terne pitagoriche; accordo con
  `<cmath>` entro qualche ulp, solo nei test.
- **Determinismo:** il job della CI che confronta i risultati sui tre sistemi
  (ADR 0008, punto 5) include questi casi.
- **Criteri di chiusura**, dal «quando è soddisfatta» della richiesta:
  - [ ] su questi casi `tan`, `acos` e `hypot` danno gli stessi bit sui tre
    sistemi operativi, entro l'errore dichiarato;
  - [ ] il responsabile verifica dal lato di Tecnigrafo che, dopo il pacchetto
    che usa le nuove funzioni, l'elenco delle eccezioni del confronto tra
    sistemi è vuoto e che `core`, `cmd` e `render` non usano più funzioni
    trigonometriche né `hypot` di `<cmath>`.
