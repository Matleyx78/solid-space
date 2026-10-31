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
