# Decisioni del responsabile

Decisioni del responsabile che cambiano le regole o le tappe, in ordine di data.
Stavano nella sezione "Decisioni" di [ROADMAP.md](ROADMAP.md) fino al
2026-10-31; si sono spostate qui perché la ROADMAP la leggono gli agenti
all'inizio di ogni sessione, e a loro la storia delle decisioni non serve: le
regole che ne derivano sono già in [AGENTS.md](../AGENTS.md),
[LAVORO-IN-PARALLELO.md](LAVORO-IN-PARALLELO.md), negli ADR e nei piani delle
tappe.

Una decisione nuova si aggiunge in fondo, con la data, un titolo in grassetto e
i rimandi ai documenti che cambia.

- **2026-10-27: nuovo scopo.** solid-space diventa il kernel di Tecnigrafo e
  cresce a tappe piccole, nell'ordine in cui serve a Tecnigrafo
  ([ADR 0001](adr/0001-scopo-e-confini.md)). Le ambizioni del primo manifesto
  (topologia cellulare, coedge propria, raccordi propri, tolleranze rigide,
  grafo e matrici sparse, disuguaglianze, parità con Parasolid) passano
  nell'appendice "Non ora" di [VISIONE.md](VISIONE.md), con il dato che le
  riporterebbe nel piano.
- **2026-10-27: OCCT dietro una facciata.** Il nucleo è OCCT, visibile solo al
  componente `kernel`. Il responsabile chiude la discussione #104 di Tecnigrafo
  (confronto tra OCCT e un nucleo proprio) con questa scelta
  ([ADR 0002](adr/0002-componenti-e-dipendenze.md)).
- **2026-10-27: l'albero delle feature sta in solid-space** (componente
  `features`, senza interfaccia grafica), così si prova da solo. Comandi,
  interfaccia, annulla e ripeti e salvataggio nel `.cadx` restano a Tecnigrafo.
- **2026-10-27: il nome resta solid-space.**
- **2026-10-27: i due progetti hanno lo stesso responsabile.** Le interfacce tra
  i due si decidono insieme, in un ADR per parte quando serve.
- **2026-10-27: le tappe nascono da richieste di Tecnigrafo.** Le tappe 0.2–1.0
  del piano iniziale non sono più impegni (il loro testo resta nella storia di
  git). Ogni tappa parte da una richiesta e da un ADR di valutazione che decide se
  OCCT basta così com'è, se va corretto o migliorato, se serve un pezzo nostro o se
  il bisogno non spetta a solid-space
  ([ADR 0007](adr/0007-tappe-su-richiesta.md), accettato).
- **2026-10-27: tappa 0.1 chiusa e citazioni di tappe corrette.** Il responsabile
  dichiara chiusa la tappa 0.1 e, poiché le tappe del piano iniziale non esistono
  più, autorizza a correggere negli ADR e negli altri documenti le citazioni di
  tappe future per numero (ADR 0003, 0004, 0005 e 0006, ciascuno con la riga
  **Corretto:**).
- **2026-10-29: un flusso unico con Tecnigrafo.** Il responsabile corregge
  l'ADR 0007, che nessun codice attua ancora: una tappa di solid-space per ogni
  tappa di Tecnigrafo che chiede qualcosa, aperta insieme a quella; la
  valutazione porta il contratto (intestazioni pubbliche in bozza); la richiesta
  nasce e vive solo qui, con il modulo `richiesta`; ogni pacchetto unito è
  utilizzabile da Tecnigrafo; la verità geometrica sta in `tests/corpus/`. Le
  richieste #20–#23 della tappa 0.7 di Tecnigrafo formano la tappa 0.2. Lo stato
  delle richieste si legge nel Project «Tecnigrafo».
- **2026-10-29: architettura del piano.** Le decisioni e le costruzioni
  geometriche del piano stanno in un componente nuovo, `geom2d`, sopra
  `robust`, che tiene predicati esatti, calcolo deterministico e politica delle
  tolleranze ([ADR 0008](adr/0008-componente-geom2d.md), accettato). Le
  richieste #20, #21, #22 e #23 hanno esito 3, serve un pezzo nostro; le
  valutazioni di #23 e #20 sono gli [ADR 0009](adr/0009-regola-delle-tolleranze-del-piano.md) e
  [0010](adr/0010-posizione-e-intersezioni-nel-piano.md), accettati con il loro
  contratto: le intestazioni pubbliche di `robust` e `geom2d` che dichiarano.
  Le valutazioni di #21 e #22 sono gli
  [ADR 0011](adr/0011-raccordi-e-tangenze-nel-piano.md) e
  [0012](adr/0012-curva-parallela-nel-piano.md), accettati con il loro
  contratto: le intestazioni `tangency.hpp` e `offset.hpp` di `geom2d`.
- **2026-10-29: decisioni per la tappa 0.2.** Preparando il piano, il
  responsabile decide: `atan2` segue lo standard C anche per gli zeri, cioè
  `atan2(±0, −0)` vale ±π (il commento di `trigonometry.hpp` si allinea con il
  pacchetto P5, issue `contratto` #31); `std::bad_alloc` è l'unica eccezione
  che può attraversare un'interfaccia pubblica ([ADR 0003](adr/0003-convenzioni.md),
  corretto); le copie dei file esterni in `tests/corpus/esterni/` sono
  obbligatorie e sono riferimenti, come in Tecnigrafo
  ([ADR 0005](adr/0005-formato-dei-casi-di-verifica.md), corretto); l'elenco dei
  casi in attesa è l'unica eccezione alla regola che lega con "Dipende da" i
  pacchetti che toccano gli stessi file
  ([LAVORO-IN-PARALLELO.md](LAVORO-IN-PARALLELO.md), "Preparare il piano"), come
  già si fa in Tecnigrafo.
- **2026-10-31: la lista dei casi in attesa diventa una cartella.** Il
  responsabile sostituisce `tests/corpus/in-attesa.txt` con
  `tests/corpus/in-attesa/`, un file per blocco di riferimenti (`0.2-R4a.txt`):
  il pacchetto di riferimenti aggiunge il suo file e quello di lavoro lo
  cancella, quindi nessun file è condiviso e cade l'eccezione alla regola
  "Dipende da" decisa il 2026-10-29, perché con un file solo 11 delle 13 pull
  request di riferimenti aperte andavano in conflitto. L'[ADR 0005](adr/0005-formato-dei-casi-di-verifica.md),
  punti 1, 4, 5 e 7, è corretto, [LAVORO-IN-PARALLELO.md](LAVORO-IN-PARALLELO.md)
  è aggiornato e la scheda della tappa 0.2 lo registra in "Modifiche al piano".
- **2026-10-31: due chiarimenti alla curva parallela.** I pacchetti di
  riferimenti R14a e R14c (issue #60 e #62) si sono fermati su due punti in cui
  il contratto non tornava, e il responsabile decide: con distanza zero
  `offsetCurve` dà la curva stessa e `offsetPolyline` dà `failed` con
  `meetsOriginal`; due curve sovrapposte non si incontrano, quindi il caso g di
  `autocad-offset.dxf` con distanza 5, dove AutoCAD chiude la giunzione con due
  archi, dà `failed` con `openJoint`. Chiudere la giunzione come AutoCAD, se
  servirà a Tecnigrafo, nascerà da una richiesta. Correggono l'[ADR
  0012](adr/0012-curva-parallela-nel-piano.md) (riga **Corretto:**), i commenti
  di `offset.hpp` e la scheda di P14.
- **2026-10-31: estremo libero e inversione di 180° nella curva parallela.** I
  pacchetti di riferimenti R14a e R14c (issue #60 e #62), ripresi dopo la
  correzione precedente, si sono fermati di nuovo su due punti in cui il
  contratto non determinava il risultato, e il responsabile decide: togliere un
  pezzo che le giunzioni girano al contrario non ripristina l'estremo del suo
  vicino, e l'estremo rimasto libero di una polilinea aperta conserva il taglio
  (la L `[(0,0),(4,0),(4,20)]` con distanza 5 dà il solo lato lungo, da (−1, 5) a
  (−1, 20)); con tangenti esattamente opposte l'arco di giunzione è la
  semicirconferenza che continua in tangenza i pezzi spostati, oraria per
  distanza positiva e antioraria per negativa, perché l'altra ripercorre la
  polilinea di partenza e di norma dà `meetsOriginal`. Correggono l'[ADR
  0012](adr/0012-curva-parallela-nel-piano.md) (riga **Corretto:**), i commenti
  di `offset.hpp` e la scheda di P14.
- **2026-10-31: Claude Max, Copilot Pro e la quota di Gemini 4 Argon.** Il
  responsabile ha il piano Max di Claude e Copilot Pro, e la quota settimanale
  di Gemini 4 Argon basta a uno o due pacchetti di riferimenti. Decide, dalla
  tappa che corrisponde alla 0.8 di Tecnigrafo (la 0.2 resta com'è), come in
  Tecnigrafo (Matleyx78/tecnigrafo#427):
  - Claude prende tutti i pacchetti impegnativi e la maggior parte dei medi, con
    Opus anche per i medi; può lavorare in più sessioni nello stesso momento con
    la sola etichetta `agente:claude` (`agente:claude-2` resta solo sulle issue
    già aperte, che prende qualunque sessione di Claude);
  - Codex scrive i riferimenti con calcoli delicati dei pacchetti di Claude, in
    fascia forte, e fa i pacchetti facili e medi che il piano non dà a Claude;
  - Gemini 4 Argon non ha pacchetti nel piano: se ha quota, il responsabile gli
    passa un pacchetto di Codex cambiandone l'etichetta;
  - Copilot serve solo come revisione mirata delle pull request impegnative,
    chiesta dal responsabile; non prende pacchetti e non apre pull request.

  Regole in [LAVORO-IN-PARALLELO.md](LAVORO-IN-PARALLELO.md) ("Chi fa cosa",
  "Modello e impegno", sezioni 2 e 6) e in [AGENTS.md](../AGENTS.md) (la tua
  etichetta, cartelle di build).
- **2026-10-31: cinque chiarimenti al contratto della curva parallela.** Su
  decisione del responsabile (issue #103, dopo l'analisi di AutoCAD, BricsCAD e
  OpenCAD Studio): al passo 3 i pezzi girati al contrario si tolgono tutti
  insieme prima di ricalcolare le giunzioni; la giunzione su un arco si
  raggiunge con la rotazione più breve dall'estremo spostato (a mezzo giro
  esatto o con ampiezza di un giro o più dà `failed` con `selfIntersection`);
  una polilinea chiusa con un solo pezzo rimasto dà `failed` con `openJoint`; se
  nessun pezzo resta dopo i passi 1, 3 e 4 l'esito è `vanished`; un pezzo
  ridotto a un punto al passo 4 rende `simple` falso; l'indice di un arco di
  giunzione tra pezzi resi consecutivi da vertici ripetuti è il vertice alla fine
  del pezzo di partenza di A. Correggono l'[ADR 0012](adr/0012-curva-parallela-nel-piano.md)
  (terza riga **Corretto:**), i commenti di `offset.hpp`, la scheda di P14 e la
  scheda della tappa 0.2 in "Modifiche al piano".
- **2026-11-01: cerchi tangenti, due chiarimenti al contratto.** Su decisione
  del responsabile (issue #66, dopo la pull request #120 del pacchetto P15): un
  centro di cerchio tangente oltre `kMaxCoordinate` (due rette quasi parallele)
  dà la diagnosi `outOfDomain` sul raggio (dato 2) e nessun cerchio, perché
  `TangentCircles` non ha un conteggio dei punti omessi e nessun cerchio si
  toglie in silenzio; il cerchio che coincide con una curva circolare (stesso
  centro, raggio uguale) la tocca ovunque e non è un cerchio tangente, quindi non
  si elenca. Correggono l'[ADR 0011](adr/0011-raccordi-e-tangenze-nel-piano.md)
  (prima riga **Corretto:**), i commenti di `tangency.hpp` (nessuna
  dichiarazione) e la scheda della tappa 0.2 in "Modifiche al piano".
- **2026-11-01: il contratto si rivede prima dei pacchetti.** Nella tappa 0.2
  cinque contratti si sono rivelati incompleti a lavori iniziati (issue #31, #66
  e #103, pull request #97 e #99), e l'ultimo è emerso in una chat che faceva
  altro, mentre un altro pacchetto lavorava sulla stessa intestazione. Da qui:
  (A) ogni tappa che realizza intestazioni del contratto comincia con il
  pacchetto `[X.Y-C]`, revisione del contratto con una lista fissa, di un
  agente diverso da chi ha scritto l'ADR (di norma Codex), appena gli ADR sono
  accettati, da cui dipendono riferimenti, lavoro e le schede di Tecnigrafo che
  usano il contratto; (B) un pacchetto non si prende se c'è una issue
  `contratto` aperta sulle sue intestazioni; (C) ogni agente rilegge il
  contratto con la lista prima di scrivere codice o casi, e se trova un buco si
  ferma senza scegliere; (D) chi cambia un'intestazione avvisa i pacchetti in
  lavorazione che la usano, e ogni pacchetto controlla prima della pull request
  che le sue intestazioni non siano cambiate in `main`. Inoltre le pull request
  chiudono le issue con `Closes #N`. Correggono l'[ADR 0007](adr/0007-tappe-su-richiesta.md)
  (punto 15, seconda riga **Corretto:**) e cambiano [AGENTS.md](../AGENTS.md),
  [LAVORO-IN-PARALLELO.md](LAVORO-IN-PARALLELO.md), [ROADMAP.md](ROADMAP.md) e i
  modelli della tappa e del pacchetto; la lista sta nel nuovo
  [CONTRATTI.md](CONTRATTI.md). Valgono dalla prossima tappa per il pacchetto
  `C`, da subito per B, C e D (nella tappa 0.2 restano P14, P16, P17 e P18).
- **2026-11-01: raccordi oltre il dominio.** Su decisione del responsabile
  (issue #70, pull request #123 del pacchetto P16): quando la regola 3 di
  `fillet` incontra un centro di cerchio tangente oltre `kMaxCoordinate`, o la
  regola 2 (raggio zero) uno spigolo oltre `kMaxCoordinate`, `fillet` dà la
  diagnosi `outOfDomain` sul raggio (dato 2), come `tangentCircles` e
  `tangentCircleNear`, e non `none`: il raccordo esiste, ma fuori dal dominio.
  Correggono l'[ADR 0011](adr/0011-raccordi-e-tangenze-nel-piano.md) (seconda
  riga **Corretto:**), il commento di `fillet` in `tangency.hpp` (nessuna
  dichiarazione), la scheda di P16 e la scheda della tappa 0.2 in "Modifiche al
  piano".
- **2026-11-01: bulge della curva parallela.** Su decisione del responsabile
  (issue #127, trovata da Codex nella revisione della pull request #124 di P14):
  il contratto di `offset.hpp` si contraddiceva, perché per un arco quasi intero
  con una corda di pochi ulp tenere il bulge mentre si arrotondano gli estremi
  sposta l'arco ben oltre `kRelativeOffsetError`, anche in `offsetCurve` (P13).
  Scelta 1 della issue: il bulge di ogni arco del risultato è quello dell'arco
  tra gli estremi arrotondati con il centro nel punto del loro asse più vicino
  al centro esatto (regola esatta in `offset.hpp`); se l'arco così costruito
  esce da `kRelativeOffsetError`, l'esito è `failed`, mai quando sta entro metà
  dell'errore: per `offsetCurve` solo con un pezzo curvo, per `offsetPolyline`
  con il nuovo motivo `notRepresentable`, aggiunto in fondo a `OffsetFailure`
  (l'unica dichiarazione che cambia). Correggono
  l'[ADR 0012](adr/0012-curva-parallela-nel-piano.md) (quarta riga
  **Corretto:**), i commenti di `offset.hpp`, le schede di P13 e P14 e la scheda
  della tappa 0.2 in "Modifiche al piano", che aggiunge R19 (codex) e P19
  (claude) per realizzarla.
- **2026-11-01: si rivedono anche le correzioni del contratto.** La pull
  request #128 ha applicato la decisione della issue #127 fissando il bulge
  della curva parallela dati gli estremi arrotondati, ma ha lasciato liberi gli
  estremi entro l'errore: cinquanta minuti dopo il merge R19 (#129) si è
  fermato perché due scelte lecite danno esiti diversi (issue #132). Nello
  stesso giorno, in Tecnigrafo, P15 (Matleyx78/tecnigrafo#359) si è fermato su
  un buco dello stesso tipo nella sua scheda: due tagli distinti con gli stessi
  valori arrotondati. Da qui: (A) ogni pull request che cambia un'intestazione
  del contratto già accettata, anche solo un commento e anche quella di un
  pacchetto `[X.Y-C]`, porta in testa **Revisione del contratto: da fare** e
  prima del merge la rivede con la lista un agente diverso da chi l'ha scritta
  (di norma Codex), con un verbale nella pull request; (B) la lista ha due
  punti nuovi: 10, scelte libere entro un errore che cambiano un esito
  discreto; 11, informazioni decise esattamente che non arrivano al chiamante;
  (C) la tappa 0.2 fa ora la revisione che non ha avuto per `offset.hpp` e
  `tangency.hpp`, con il pacchetto `[0.2-C]` (#133), da cui dipendono R19 e
  P19. Correggono l'[ADR 0007](adr/0007-tappe-su-richiesta.md) (punto 15,
  terza riga **Corretto:**) e cambiano [CONTRATTI.md](CONTRATTI.md),
  [AGENTS.md](../AGENTS.md), [LAVORO-IN-PARALLELO.md](LAVORO-IN-PARALLELO.md) e
  la scheda della [tappa 0.2](tappe/0.2-geometria-del-piano.md) in «Modifiche
  al piano».
- **2026-11-01: decisioni della revisione C del contratto (#133).** Il
  responsabile decide nelle issue #132 e #135–#144 e nei commenti della #133:
  - **#132:** ogni vertice restituito dall'offset, anche dritto, tagliato,
    prolungato o di giunzione, è il double più vicino per coordinata, pareggio
    al pari. Bulge e rappresentabilità si calcolano su quei double. Estremi
    arrotondati coincidenti di un arco non nullo danno `failed` per
    `offsetCurve`, `failed` / `notRepresentable` per `offsetPolyline`, senza
    scartare l'arco come puntiforme. R19 e P19 si allargano a tutti i vertici
    e ai percorsi già uniti di P13/P14, difficoltà invariata.
  - **#135:** dopo le due tangenze esatte, `TangentCircles::circles` si ordina
    per centro esatto, x e poi y; `tangentCircleNear` e `fillet` ereditano il
    primo della lista a parità di punteggio. Nell'esempio il primo centro è
    (−0,5, 0), come nel codice già unito.
  - **#136, #137, #138, #143, #144:** ogni costruzione restituita è nel dominio
    di `curves.hpp`: coordinate, raggi e punto m dei pezzi curvi
    entro `kMaxCoordinate`. Si controllano i dati, mai l'estensione geometrica;
    nessuna costruzione fuori dominio si restituisce o si omette in silenzio.
    `tangentsFrom` dà `outOfDomain` sul dato 1 per tangenze fuori dominio;
    `tangentCircles`, `tangentCircleNear` e `fillet` sul dato 2 per centri o
    tangenze, con fallimento dell'intera chiamata. In `fillet` vale anche per
    regola 1 (centro, raggio costruito, tangenze e piede), spigoli di regola 2
    e punto m della forma con bulge. `offsetCurve` dà `outOfDomain` sul dato 1
    per risultato fuori dominio (anche raggio e punto m); zero conserva la curva
    valida e i suoi bit, con `done`. `offsetPolyline` controlla soltanto i dati
    del risultato finale dopo i passi 1–4, con `failed` / `beyondDomain`;
    intermedi poi tagliati o rimossi non contano, nessuna diagnosi di risultato
    ereditata da `offsetCurve`. La garanzia dei casi semplici richiede anche
    il risultato esatto e il punto m dai double restituiti nel dominio.
  - **#139, #140:** regola generale di `curves.hpp`, per tutti i limiti di
    errore geom2d: scala max(M, |p|, 2^-1022), o max(M, |s|, 2^-1022) per
    stazioni e lunghezze. Nei subnormali il limite dei punti è 2^-1070,
    quello di stazioni e curve 2^-1068. Nessun cambiamento delle decisioni
    esatte, del dominio o delle diagnosi. R20 (#146, codex) scrive i casi;
    P20 (#145, claude) li rende obbligatori e corregge solo se falliscono,
    senza cambiare contratto né predicati esatti. P18 dipende anche da P20.
  - **#141, #142:** regola unica in «Checking the data» di `curves.hpp`:
    ingressi per numero di `Diagnosis::input`, ciascuno per intero; prima
    diagnosi applicabile, dentro una curva invalidValue, outOfDomain,
    degenerateCurve. Polilinea per pezzi in ordine di `pieces()`, primo pezzo
    errato con vertice nel messaggio; scartati i pezzi puntiformi, nessun
    pezzo rimasto dà degenerateCurve sul dato 0 prima degli ingressi seguenti.
    Valori non appartenenti a un pezzo, come l'ultimo bulge aperto, ignorati.
    Diagnosi specifiche come notCircular seguono i dati dello stesso ingresso
    e precedono gli ingressi seguenti; tutti i dati prima delle costruzioni
    (#150), anche quando queste portano un input precedente. Per fillet il raggio si valida anche
    con rette parallele, prima delle regole geometriche.
  Correggono i soli commenti di `curves.hpp`, `offset.hpp`, `tangency.hpp`, gli
  ADR 0010, 0011 e 0012 con riga **Corretto:** e le schede interessate; C è esteso
  ai commenti generali di `curves.hpp`. Il piano aggiunge R20/P20 e allarga
  R19/P19. Le dichiarazioni e i casi di riferimento non cambiano in C. La
  pull request unica chiude C e i buchi, e passa la revisione delle correzioni
  da un agente diverso dall'autore prima del merge (CONTRATTI.md).

- **2026-11-01: correzioni dopo la revisione della PR #147, decisioni #148–#152.**
  Il responsabile distingue il principio del dominio dai risultati degeneri,
  regolati funzione per funzione (#132, #148, #152):
  - **#148:** pezzo dritto non degenere con estremi arrotondati coincidenti:
    `offsetCurve` dà `failed`; `offsetPolyline` lo scarta dopo i vertici e
    prima dei bulge e dei controlli finali, con la sua origine e `simple`
    falso. Se non resta nulla, `failed` / `notRepresentable`, non `vanished`.
    La garanzia dei casi semplici esclude anche il collasso per arrotondamento.
  - **#149:** dominio sul valore esatto di coordinate, centri, vertici,
    tangenze e raggi, anche se il double più vicino tornerebbe entro K.
    Un esatto entro K non si restituisce oltre K: se serve si limita a ±K
    l'arrotondamento. Nessun nuovo obbligo del double più vicino per centri e
    tangenze. Il punto m dei pezzi curvi restituiti si ricostruisce esattamente
    dai double restituiti. In `offsetPolyline` contano solo i vertici esatti
    finali dopo i passi 1–4; gli altri controlli restano sui double restituiti.
  - **#150:** tutti i dati, in ordine di input e con le diagnosi specifiche
    come `notCircular`, prima delle costruzioni, anche se la diagnosi di
    costruzione porta un input precedente. In regola 3 di `fillet`, prima
    `ambiguousPick`, poi le costruzioni, `infinitelyMany` e i candidati.
  - **#151:** `tangentsFrom` controlla solo i punti elencati: con `none` un
    punto escluso dall'arco, anche oltre K, non fa fallire; con `extended`
    tutti i punti sono controllati. Non cambia l'eredità di `tangentCircles`
    nei chiamanti, che falliscono anche per un candidato non scelto.
  - **#152:** se `onFirst.point` e `onSecond.point` restituiti coincidono,
    bulge 0, `Arc` col raggio dato e le proprie regole. La coincidenza può
    dipendere dalla scelta lecita dei punti entro l'errore: un riferimento
    non impone uno dei due rami. Si verifica condizionatamente; se il formato
    non lo permette, test di unità in P20. La forma con bulge può essere un
    punto: Tecnigrafo usa allora l'`Arc`.
  L'ADR 0010 entra nei file autorizzati di C, con riga **Corretto:** e
  pavimento 2^-1022 anche per `endpoints` e `length`; P7, P10 e P11 restano
  invariate. «First piece with invalid data» chiarisce lo scarto dei pezzi
  esattamente puntiformi. I controlli finali di `offsetPolyline` seguono
  l'ordine dell'elenco, primo fallimento decisivo, prima `beyondDomain`.
  R19/P19 coprono #148 e #149 (e P19 corregge l'ordine finale se necessario).
  R20 aggiunge due o tre casi subnormali di `endpoints` e `length` (arco e
  segmento), confine K ed esempi #150–#152; il testo della #146 lo aggiorna
  il responsabile. P20 aggiunge `curves.cpp` ai file solo se servono e copre
  #149–#152 se il codice già unito non le rispetta, difficoltà impegnativa,
  `Contratto:` invariato. Nessuna implementazione o caso modificato da C.
  Le decisioni entrano nella stessa PR #147, con `Closes #148`–`Closes #152`,
  da rivedere di nuovo sulle sole funzioni toccate secondo CONTRATTI.md.

- **2026-11-01: ulteriori decisioni della revisione della PR #147 (#153–#156).**
  - **#153:** in `offsetCurve`, estremi e raggio esatti fuori dominio danno
    `outOfDomain`, dato 1, prima di `failed`, anche con estremi arrotondati
    coincidenti o bulge non rappresentabile. `failed` vale solo con quei
    valori esatti nel dominio; m si controlla dai double solo per una curva
    restituita. La polilinea scarta invece prima i dritti collassati e
    controlla solo il risultato finale: lo stesso pezzo da solo può dare
    `failed` / `notRepresentable`. P13, P19 e R19 allineate; P19 corregge
    l'ordine già unito se necessario, R19 aggiunge l'esempio curva/polilinea.
  - **#154:** dominio esatto della polilinea sugli estremi dei pezzi rimasti
    dopo lo scarto dei dritti collassati. Ai vertici condivisi si controllano
    entrambi i precursori esatti, fine del precedente e inizio del successivo;
    estremi solo di pezzi scartati non contano. m resta dai double restituiti.
    P14, P19 e R19 allineate; P19 adegua se serve, R19 aggiunge l'esempio.
  - **#155:** in `fillet`, tangenze e bulge liberi entro l'errore possono dare
    m ricostruito entro o oltre K: al confine sono leciti `found` oppure
    `outOfDomain` sul dato 2. Un riferimento non impone l'esito: con `found`
    punti e arco entro gli errori e m entro K; con `outOfDomain`, dato 2.
    Nessun obbligo del double più vicino o di un m sempre entro K. P16 e P20
    allineate; P20 non cambia codice per questa decisione e R20 non impone
    l'esito al confine, come nella #152.
  - **#156:** il principio del dominio di `curves.hpp` (esatto, limite ±K,
    m dai double, nessuna omissione silenziosa) vale solo per costruzioni di
    `offset.hpp` e `tangency.hpp`. Gli estremi di `endpoints` e delle
    sovrapposizioni di `intersect` e `intersectAlong` restano entro l'errore
    anche oltre K; i punti comuni isolati oltre K restano omessi e contati
    (`omittedBeyondDomain`). `endpoints`, `intersect`, `intersectAlong` e
    `intersection.hpp` invariati; nessun pacchetto nuovo, P7/P10/P11 invariate.
  Solo commenti delle intestazioni, ADR 0010–0012, schede elencate dalle
  decisioni e questo registro nella stessa PR #147, con `Closes #153`–
  `Closes #156`. Nessuna dichiarazione, implementazione o caso modificato.
  La nuova revisione del contratto sulle sole funzioni toccate spetta a un
  agente diverso dall'autore, prima del merge.

- **2026-11-02: fascia di confine di fillet (#157).** Su decisione del
  responsabile nella #157, la fascia è quella dell'errore dell'arco:
  `kRelativeArcError * max(M, |p|, 2^-1022)`. Quando un punto dell'arco esatto
  ha una coordinata il cui valore assoluto supera K meno quell'errore, una
  scelta lecita può dare `found` oppure `outOfDomain` sul dato 2 per m
  ricostruito; più lontano dal confine, m ricostruito è nel dominio.
  Cambiano soltanto la frase nel commento di `fillet` in `tangency.hpp`,
  l'ADR 0011 (punto 8 e riga **Corretto:**), le schede P16 e P20 e questo
  registro, nella stessa PR #147 con `Closes #157`. Il resto del commento e
  tutte le dichiarazioni restano invariati. P20 non cambia codice; R20 non
  impone l'esito dentro la fascia.

- **2026-11-02: esatto dove serve (operazione snellimento, #160).** Tre
  decisioni e un principio, nell'[ADR 0013](adr/0013-esatto-dove-serve.md):
  1. restano esatte le decisioni (lato, incontri, coincidenza, diagnosi e loro
     ordine, pezzi della curva parallela che sopravvivono); le costruzioni si
     garantiscono solo entro gli errori già dichiarati, senza dire quale
     `double` esce; restano il determinismo, i valori copiati o fissati per
     struttura, il confine esatto di `kMaxCoordinate` e le costanti angolari;
  2. il dominio in basso è 2^-50: il pavimento 2^-1022 cade, le formule
     degli errori tornano max(M, |p|) e max(M, |s|), e i limiti valgono per
     dati di grandezza M almeno 2^-50 (una sola frase, in testa a
     `curves.hpp`); sotto 2^-50 si garantiscono solo niente crash, niente
     risultati fuori dominio e il determinismo; i subnormali escono dal
     contratto delle costruzioni, mentre le decisioni di `robust` e `geom2d`
     restano esatte per ogni `double` finito;
  3. un buco è visibile quando cambia l'esito o la diagnosi, il numero o
     l'ordine delle entità, o una coordinata di più di epsilon (ADR 0009): lo
     decide il responsabile; gli altri li sceglie chi implementa, nel modo più
     semplice, scritto nel commento dell'intestazione, fissato da un test e
     segnalato nella pull request;
  4. un esito che dipende solo da quale `double` una costruzione sceglie entro
     il suo errore è libero dentro la sua fascia, deterministico, e nessun caso
     del corpus lo impone; per la curva parallela la fascia è quella degli
     estremi esatti a meno di due errori di punto, e degli archi quasi interi
     con la corda di pochi ulp; le #152, #155 e #157 di `fillet` ne sono
     applicazioni.

  Cambiano l'ADR 0013 (nuovo), una riga **Corretto:** negli ADR 0010–0012,
  [AGENTS.md](../AGENTS.md) e [CONTRATTI.md](CONTRATTI.md) (la regola 3 e il
  punto 10 della lista). Seguono, in pull request separate: i commenti di
  `curves.hpp`, `offset.hpp` e `tangency.hpp` (`intersection.hpp` resta
  com'è), i casi del corpus con valori costruiti, il controllo di simmetria di
  `tangentCircles` e il piano 0.2 (P19 senza l'obbligo del `double` più
  vicino, R19 di conseguenza; P20 «dominio delle tangenze» #149–#151, R20 con
  i soli casi al confine `kMaxCoordinate`; C chiusa).

- **2026-11-02: estremi delle linee allineate unite (#158).** Su decisione
  del responsabile nella #158 (buco trovato dalla P22 della tappa 0.7 di
  Tecnigrafo, Matleyx78/tecnigrafo#373 punto 3, e classificato come buco nel
  contratto in Matleyx78/tecnigrafo#476): `JOIN` unisce linee allineate dal
  primo all'ultimo estremo lungo la sorgente, ma nessuna funzione del
  contratto ordinava quegli estremi, e due estremi possono avere la stessa
  proiezione esatta. Scelta 2 della issue: `coincidence.hpp` dà direttamente
  i due estremi del segmento unito con la funzione nuova `joinAligned`
  (l'unica dichiarazione che cambia). Gli estremi sono quelli della sorgente,
  poi quelli di ogni altro segmento nell'ordine dato; lungo la sorgente vuol
  dire per (p − inizio) · (fine − inizio), deciso esattamente; a parità
  esatta vince il primo in quell'ordine, la sorgente prima, sia per l'inizio
  sia per la fine. Nessuna tolleranza, nessun controllo dell'allineamento,
  punti restituiti come sono nei dati; diagnosi dei dati di ogni segmento
  (sorgente dato 0, `others[i]` dato i + 1). Correggono
  l'[ADR 0009](adr/0009-regola-delle-tolleranze-del-piano.md) (riga
  **Corretto:**, punto 5 e Contratto), i commenti di `coincidence.hpp` e la
  scheda della tappa 0.2 in "Modifiche al piano", che aggiunge R21 (codex) e
  P21 (claude) per realizzarla.

- **2026-11-03: AGENTS.md a nucleo e due regole nuove (operazione snellimento,
  #160, passo 4).** [AGENTS.md](../AGENTS.md) diventa un nucleo sempre letto
  (dove si scrive, presa in carico, regole che non si possono violare, quando
  fermarsi con «Buchi visibili e no», prima di dichiarare finito, commit e pull
  request) e una tabella «Se ... leggi anche ...» con segni oggettivi, che
  instrada agli otto file di [agenti/](agenti/), con gli stessi nomi in
  Tecnigrafo. Il testo uscito dal nucleo sta lì, sotto i titoli che aveva o in
  sezioni che dicono da dove viene, con i soli rimandi adeguati; una sezione
  citata come AGENTS.md, "<titolo>", che il nucleo non ha, sta nel file con lo
  stesso titolo. [GEMINI.md](../GEMINI.md) rimanda al nucleo e alla
  tabella. Le regole nuove:
  1. una pull request che tocca solo documenti, commenti o casi di riferimento
     non si compila in locale: la verifica la fa la CI (nucleo, «Prima di
     dichiarare il lavoro finito»; [comandi.md](agenti/comandi.md),
     [riferimenti.md](agenti/riferimenti.md), [chiusura.md](agenti/chiusura.md));
  2. nella revisione, un caso che sta entro un errore dichiarato dal confine di
     `kMaxCoordinate` o da un'altra soglia del contratto si chiude dichiarando
     la fascia ([ADR 0013](adr/0013-esatto-dove-serve.md), punto 4): il
     revisore lo segnala come non bloccante, senza issue `contratto` e senza
     decisione del responsabile; l'autore scrive la fascia nella stessa pull
     request, nel commento dell'intestazione, senza un altro giro di revisione
     ([revisione.md](agenti/revisione.md), [contratto.md](agenti/contratto.md)).

- **2026-11-03: analisi notturna con SonarQube Cloud (#172).** Il responsabile
  aggiunge un'analisi di SonarQube Cloud del solo ramo `main`, ogni notte verso
  le 02:00 italiane (cron `17 1 * * *`, in UTC) e a mano con
  `workflow_dispatch`, nel workflow separato `.github/workflows/sonar.yml`, che
  non tocca `ci.yml`. Nessuna analisi sulle pull request né sui push, nessuna
  copertura dei test per ora. L'azione `SonarSource/sonarqube-scan-action` è
  fissata a uno SHA preciso; il C++ si analizza dal `compile_commands.json` del
  preset `release` (`build/release/compile_commands.json`), senza Build
  Wrapper, con la configurazione in `sonar-project.properties`. L'analisi
  automatica di Sonar è spenta e il token sta nel secret `SONAR_TOKEN`.

- **2026-11-04: rappresentabilità degli archi di offset (#177).** Il responsabile
  sceglie l'opzione 2: estremi dei pezzi curvi entro 2^-51 · max(M, |p|) per
  coordinata, anche ai vertici condivisi con un pezzo dritto. Con estremi
  distinti e limiti d'errore applicabili, fino a 2π − 1/4 radianti il bulge
  non causa un fallimento di rappresentabilità. P19 dimostra la soglia e
  prova la fascia oltre di essa. Restano i controlli per estremi coincidenti
  e gli altri motivi di fallimento. I casi R19 già pubblicati restano validi.

- **2026-11-04: scala dell'errore dell'offset per pezzo (#181).** Il responsabile
  sceglie l'opzione 1. La distanza ammessa in entrambe le direzioni per un
  pezzo finale è `kRelativeOffsetError` · max(M, S), con S massimo valore
  assoluto delle coordinate su tutto il pezzo esatto. I limiti dei vertici
  mantengono la scala del singolo vertice. La soglia della #177 si verifica
  con questa scala in P19. I casi R19 restano applicabili.

- **2026-11-04: parte di ciascuna curva tenuta dal raccordo (#184).** Su
  richiesta della P19 della tappa 0.7 di Tecnigrafo (Matleyx78/tecnigrafo#367),
  dove l'agente si era fermato: la `Location` dei punti di tangenza non dice se
  si tiene la parte dall'inizio al punto o dal punto alla fine, né, per un arco
  con il punto `afterEnd`, quale estremo si prolunga. Il responsabile sceglie la
  strada B2, il contratto lo dice per ciascuna curva, e decide:
  1. ogni curva tiene la parte che prosegue l'arco di raccordo nel suo punto di
     tangenza, come AutoCAD;
  2. se il punto di tangenza è fuori dalla curva, si prolunga l'estremo che
     prosegue il raccordo, come AutoCAD (nel caso e di `autocad-raccordi.dxf`
     è l'inizio dell'arco): non è una regola fissa;
  3. con raggio zero (spigolo) o punti di tangenza coincidenti, ogni curva
     tiene la parte dalla parte del punto indicato rispetto allo spigolo, come
     CHAMFER;
  4. con le curve scambiate o i versi invertiti, la stessa parte geometrica.

  La forma del dato la propone la pull request del contratto: `KeptPart`
  (`fromStart`, `toEnd`) e i campi `keepFirst` e `keepSecond` di `Fillet`,
  decisi esattamente dalla regola 5 di `fillet` in `tangency.hpp`. Correggono
  l'[ADR 0011](adr/0011-raccordi-e-tangenze-nel-piano.md) (riga **Corretto:**)
  e la scheda della tappa 0.2 in "Modifiche al piano", che aggiunge R22 (codex)
  e P22 (claude) per realizzarla; Matleyx78/tecnigrafo#367 aspetta anche P22.

- **2026-11-04: precisazioni al contratto di `fillet` (#185).**
  - **#186:** la simmetria della #184 vale a parità di geometria scelta.
    Restano il pareggio a favore della fine nella regola 1 e il verso
    antiorario dalla prima alla seconda tangenza nella regola 4.
  - **#187:** se la parte che prosegue il raccordo richiederebbe di far
    tornare indietro la curva oltre l'estremo da conservare, `fillet` dà
    `radiusTooLarge` senza raccordo. Tecnigrafo mostra «Radius is too large.»
    e lascia il disegno com'è.
  - **#188:** con raggio zero o tangenze esattamente coincidenti, per un
    arco si tiene la parte che contiene il punto indicato, secondo l'ordine
    lungo la curva. Se lo spigolo è fuori dall'arco e le due parti contengono
    il punto, si prolunga l'estremo con il prolungamento angolare minore,
    confrontato esattamente. In parità esatta, `ambiguousPick`. Per le
    rette vale CHAMFER.

- **2026-11-04: come si rivede (blocco di P19).** Il responsabile decide due
  regole per le revisioni, la prima perché fermava P19:
  1. una garanzia del contratto si verifica con i test, non con dimostrazioni
     matematiche: il revisore che teme un ramo non coperto chiede un test che lo
     raggiunga, non un lemma (lo stesso nella #489 di Tecnigrafo);
  2. il revisore serve a far arrivare il lavoro al merge, non a fermarlo: blocca
     solo un errore vero o una regola persa. Se una regola esistente risponde, la
     applica e va avanti; i dubbi di forma li scrive come non bloccanti. Prima di
     dichiarare un buco controlla se una regola esistente lo decide già (dati
     fuori dominio, ADR 0035 di Tecnigrafo, punto 23; pezzi degeneri, ADR 0036 di
     Tecnigrafo, punto 5; fasce, [ADR 0013](adr/0013-esatto-dove-serve.md), punto
     4): se sì, lo scrive come applicazione di quella regola, non come buco, e non
     blocca il merge. Dati privi di senso pratico (lunghezze sotto la tolleranza,
     ampiezze di un ulp) si trattano con queste regole.

  Regole in [revisione.md](agenti/revisione.md) («A che cosa serve la revisione»,
  «Prima di dichiarare un buco», «Test, non dimostrazioni»).

- **2026-11-05: impostazioni dei repository e README in inglese.** Il
  responsabile decide che solid-space e Tecnigrafo abbiano le stesse protezioni,
  salvo i controlli di stato che differiscono (Qt, verifica):
  1. si unisce solo con squash; il ruleset di `main` richiede i controlli di
     stato, una pull request senza approvazioni obbligatorie e la cronologia
     lineare, e l'amministratore lo aggira solo tramite pull request, non con un
     push diretto; un ruleset sui tag `v*` vieta di cancellarli o spostarli,
     senza eccezioni;
  2. segnalazione privata delle vulnerabilità e avvisi Dependabot attivi (non gli
     aggiornamenti automatici dei sottomoduli, fissati di proposito); la scheda
     del repository ha descrizione e topic;
  3. il README si apre con un paragrafo in inglese: eccezione alla regola
     «documenti in italiano», limitata a quel paragrafo.

  Le impostazioni di GitHub le applica il responsabile; l'elenco dei controlli
  obbligatori resta quello di `docs/MANUTENZIONE.md` di Tecnigrafo, da
  aggiornare con le regole nuove.

- **2026-11-05: runner di Windows e macOS.** Il responsabile decide che i
  runner di Windows e macOS restano `windows-latest` e `macos-latest`, non
  fissati a `windows-2022` e `macos-15`: il codice è scritto per le immagini
  attuali e su quelle precedenti la CI fallisce (prova nella pull request #199,
  chiusa senza unire). Se una migrazione delle immagini rende rossa la CI, si
  corregge il codice o si fissa l'immagine con una nuova decisione. Registrata
  nell'[ADR 0006](adr/0006-build-ci-e-sottomoduli.md), riga **Corretto:**.

- **2026-11-05: runner fissati alle immagini più recenti.** Il responsabile
  corregge la decisione precedente sui runner: Windows e macOS non restano su
  `latest` ma sono fissati a `windows-2025-vs2026` e `macos-26`, le più recenti,
  così il compilatore cambia solo quando lo decide lui. Linux resta
  `ubuntu-24.04`. L'allineamento di Tecnigrafo alle stesse immagini è una
  decisione di quel repository. Nell'[ADR 0006](adr/0006-build-ci-e-sottomoduli.md),
  seconda riga **Corretto:**.
