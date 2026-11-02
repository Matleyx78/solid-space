# ADR 0012: Curva parallela di linee, archi, cerchi e polilinee del piano

- **Stato:** Accettato
- **Data:** 2026-10-29
- **Corretto:** 2026-10-31 — due chiarimenti, decisi dal responsabile quando i
  pacchetti di riferimenti R14a e R14c (issue #60 e #62) hanno trovato due punti
  in cui il contratto non tornava: la distanza zero (punto 2) e le curve
  sovrapposte nelle giunzioni (punto 4), che decide il caso g di
  `autocad-offset.dxf` con distanza 5 (valutazione, «Su che cosa si basa»).
- **Corretto:** 2026-10-31 — altri due chiarimenti, decisi dal responsabile quando
  gli stessi pacchetti di riferimenti, ripresi dopo il primo, si sono fermati di
  nuovo (issue #60 e #62) su due punti in cui il contratto non determinava il
  risultato: l'estremo che resta libero quando si toglie un pezzo e il verso
  dell'arco di giunzione a tangenti esattamente opposte (punto 4; tabella, caso 3;
  «Verifica»).
- **Corretto:** 2026-10-31 — cinque punti che il contratto non determinava,
  decisi dal responsabile dopo l'analisi di AutoCAD, BricsCAD e OpenCAD Studio
  (issue #103): (1) i pezzi girati al contrario al passo 3 si tolgono tutti
  insieme prima di rifare le giunzioni; (2) su un arco la giunzione si raggiunge
  con la rotazione più breve dall'estremo spostato, e a mezzo giro esatto o con
  ampiezza di un giro o più dà `failed` con `selfIntersection`; (3) una
  polilinea chiusa con un solo pezzo rimasto dà `failed` con `openJoint`, e se
  nessun pezzo resta dopo i passi 1, 3 e 4 l'esito è `vanished`; (4) un pezzo
  ridotto a un punto al passo 4 rende `simple` falso; (5) l'indice dell'arco di
  giunzione tra pezzi resi consecutivi da vertici ripetuti è il vertice alla
  fine del pezzo di partenza di A (punti 3, 4, 6 e 8).
- **Corretto:** 2026-11-01 — un punto in cui il contratto si contraddiceva,
  trovato nella revisione del pacchetto P14 (issue #127) e deciso dal
  responsabile: per un arco quasi intero con una corda di pochi ulp, tenere il
  bulge mentre si arrotondano gli estremi sposta l'arco ben oltre
  `kRelativeOffsetError`. Il bulge di ogni arco del risultato si ricostruisce
  dagli estremi arrotondati, con il centro più vicino a quello esatto; se
  nessun bulge rappresenta l'arco entro l'errore, l'esito è `failed`, con il
  nuovo motivo `notRepresentable` per le polilinee (punti 3, 5, 6 e 9).
- **Corretto:** 2026-11-01 — decisioni della revisione C (#133), issue #132,
  #138, #139, #141, #143 e #144: tutti i vertici dell'offset al double più
  vicino, pareggio al pari; estremi curvi arrotondati coincidenti danno
  `failed` / `notRepresentable`; dominio sui dati restituiti, soltanto finali
  per le polilinee; pavimento 2^-1022 degli errori e ordine delle diagnosi.
  R19/P19 sono allargati, R20/P20 verificano e realizzano i subnormali.
  La revisione delle correzioni (#148, #149 e #150) precisa: pezzi dritti
  collassati dall'arrotondamento, dominio sul valore esatto e punto m dai
  double restituiti, nessun valore restituito oltre K, dati prima delle
  costruzioni e ordine dei controlli finali (punti 3–6 e 9).
  Le #153 e #154 precisano: in `offsetCurve` dominio di estremi e raggio
  esatti prima di `failed`; nella polilinea solo estremi dei pezzi rimasti
  dopo lo scarto dei dritti collassati, con entrambi i precursori esatti
  dei vertici condivisi (punto 9).
- **Richiesta:** #22 — da Tecnigrafo: tappa 0.7, Matleyx78/tecnigrafo#328
- **Esito:** 3 pezzo nostro
- **Tappa:** 0.2 (scheda da scrivere all'apertura)

## Richiesta

Tecnigrafo chiede la curva parallela a una distanza data, dal lato indicato da
un punto, di una linea, di un arco, di un cerchio e di una polilinea aperta o
chiusa fatta di segmenti e archi: il risultato di `OFFSET` di AutoCAD con
`OFFSETGAPTYPE=0` (giunzioni chiuse prolungando i pezzi, pezzi che si
annullano tolti). Nei casi semplici il risultato deve essere sempre giusto,
entro un errore dichiarato e senza autointersezioni; negli altri, un risultato
senza autointersezioni o una risposta che dice che non è riuscito, mai un
risultato sbagliato in silenzio. Tecnigrafo deve sapere che cosa è garantito,
per scriverlo al suo utente. Il testo completo è nella issue #22.

## Valutazione

- **Spetta a solid-space:** la curva parallela è un compito di `geom2d`
  ([ADR 0008](0008-componente-geom2d.md), punto 2, prerequisito di questa
  valutazione), e le giunzioni sono intersezioni dell'[ADR
  0010](0010-posizione-e-intersezioni-nel-piano.md): devono venire dalle stesse
  decisioni esatte.
- **Esiti 1 e 2 esclusi da un vincolo già preso:** `core` di Tecnigrafo compila
  senza librerie esterne, quindi senza OCCT (ADR 0030 di Tecnigrafo, punto 5;
  [ADR 0007](0007-tappe-su-richiesta.md), punto 8).
- **Esito 3**, in `geom2d`, sopra le curve, le intersezioni e il lato dell'ADR
  0010 e la regola delle tolleranze dell'[ADR
  0009](0009-regola-delle-tolleranze-del-piano.md).
- **Che cosa vuol dire "giusto".** La richiesta dà la regola di AutoCAD, non una
  definizione. Questo ADR ne scrive una, sui valori esatti, in quattro passi
  (`offset.hpp`): ogni pezzo si sposta; due pezzi vicini si uniscono nel loro
  punto comune più vicino al vertice, o con un arco centrato nel vertice se le
  curve prolungate non si incontrano; un pezzo che le giunzioni girano al
  contrario si toglie; i pezzi ridotti esattamente a un punto si tolgono. Nei
  casi semplici è il comportamento di AutoCAD osservato in
  `autocad-offset.dxf` e riassunto nella bozza dell'ADR 0035 di Tecnigrafo,
  punto 8 (Matleyx78/tecnigrafo#330).
- **Su che cosa si basa:** il testo della richiesta, il calcolo a mano dei suoi
  casi (sotto) e, come informazione, la descrizione dei casi di
  `autocad-offset.dxf`. **Non verificato:** nessun prototipo è stato eseguito;
  le prestazioni sono un obiettivo da misurare. Il caso g di
  `autocad-offset.dxf` (l'arco che sparisce con distanza 5), che non si era
  potuto confrontare con i quattro passi senza le sue coordinate, è stato
  confrontato dal pacchetto R14a (issue #60, 2026-10-31): AutoCAD chiude la
  giunzione con due archi di raggio 5, centrati negli estremi dell'arco sparito,
  che si incontrano in (473, 4); i passi davano invece una retta, perché i due
  segmenti spostati stanno sulla stessa retta y = 5, i cui punti di mezzo distano
  5,83 dalla polilinea di partenza e non 5. Decide il responsabile (punto 4): due
  curve sovrapposte non si incontrano, e il risultato è `failed` con
  `openJoint`. Chiudere la giunzione come AutoCAD non si garantisce (punto 12) e,
  se servirà a Tecnigrafo, nascerà da una richiesta.

### I casi della richiesta

| Caso | Risposta |
|---|---|
| 1. Rettangolo chiuso 100×50, `d` = 10, verso l'interno e l'esterno | semplici: il rettangolo 80×30 dentro e 120×70 fuori, con gli spigoli vivi, perché due lati consecutivi prolungati si incontrano sempre |
| 2. Asola (due segmenti paralleli e due semicerchi tangenti), verso l'interno e l'esterno | semplici se `d` è minore del raggio R dei semicerchi verso l'interno, e sempre verso l'esterno: l'asola con semicerchi di raggio R − `d` o R + `d`. Nei punti di tangenza i pezzi spostati hanno lo stesso estremo esatto, quindi non serve una giunzione. Verso l'interno con `d` ≥ R i semicerchi si annullano e le due rette spostate non si incontrano o coincidono: `failed` |
| 3. Polilinea a L con un lato più corto di `d`, verso l'interno dell'angolo | non semplice: il lato corto, tagliato dalla giunzione, gira al contrario e si toglie (passo 3); il risultato è quello dei passi, senza autointersezioni, oppure `failed`. Il vicino resta tagliato dalla giunzione (punto 4): la L `[(0,0),(4,0),(4,20)]` con `d` = 5 dà il solo lato lungo, da (−1, 5) a (−1, 20) |
| 4. Polilinea con un arco interno di raggio minore di `d` | non semplice: l'arco si annulla (passo 1, confronto esatto tra raggio e distanza) e i vicini si uniscono nel loro punto comune; se non si incontrano, `failed` con `openJoint` |
| 5. Polilinea a U stretta, `d` maggiore di metà larghezza | il fondo gira al contrario e si toglie, i due lati spostati sono paralleli e non si incontrano: `failed` con `openJoint`. AutoCAD non crea nulla (caso f di `autocad-offset.dxf`) |
| 6. Gli stessi casi a coordinate dell'ordine di 1e6 | le stesse risposte per i dati traslati, che sono già arrotondati; gli errori sono relativi a M = 1e6 (circa 3.6e-9 per i vertici, 1.4e-8 per le curve) |

## Alternative considerate

- **Esito 1 o 2 (OCCT, `BRepOffsetAPI_MakeOffset`):** esclusi dall'ADR 0030 di
  Tecnigrafo, punto 5.
- **Esito 4, la curva parallela in Tecnigrafo:** le giunzioni sono
  intersezioni, e i controlli sulle autointersezioni sono le decisioni esatte
  dell'ADR 0010; rifarli in Tecnigrafo significherebbe due implementazioni
  delle stesse decisioni.
- **Un algoritmo generale** (curva grezza, poi taglio delle parti più vicine
  della distanza, come le librerie di offset di poligoni): copre più casi non
  semplici, ma è un pezzo molto più grande da scrivere e da provare, e la
  richiesta chiede garanzie solo nei casi semplici. Se Tecnigrafo ne avrà
  bisogno, nascerà da una richiesta; il contratto non lo impedisce, perché
  negli altri casi promette solo "un risultato senza autointersezioni o
  `failed`".
- **Una libreria esistente** (Clipper2, licenza Boost): lavora su poligoni a
  coordinate intere e approssima gli archi con segmenti, mentre il risultato
  deve tornare una polilinea con archi; e `core` non ha dipendenze esterne.
- **Restituire il risultato anche quando si autointerseca**, lasciando il
  controllo a Tecnigrafo: un risultato sbagliato potrebbe uscire in silenzio,
  che la richiesta esclude.
- **Il controllo delle autointersezioni del tutto esatto anche tra pezzi
  consecutivi:** una retta tangente a un arco in un vertice, dopo
  l'arrotondamento, può toccarlo o tagliarlo di nuovo a una distanza minima dal
  vertice; il controllo esatto darebbe `failed` proprio nei casi comuni (l'asola
  del caso 2). Per i pezzi consecutivi vale la regola dell'ADR 0009, e per
  questo la funzione riceve la `TolerancePolicy`.
- **Il lato come punto, dentro `geom2d`:** per una polilinea il lato dipende dal
  pezzo più vicino al punto, cioè da una proiezione, che resta a Tecnigrafo
  (ADR 0010, punto 9). La distanza con segno, sinistra positiva come in
  `side()`, tiene il contratto piccolo; Tecnigrafo ottiene il segno con
  `side()` sul pezzo che sceglie.
- **Una diagnosi per `failed` e `vanished`:** non sono errori nei dati ma
  risposte. Sono valori (`OffsetOutcome`, `OffsetFailure`), e i codici di
  diagnosi dell'ADR 0010 non cambiano.

## Decisione

1. **La curva parallela sta in `geom2d`**, nell'intestazione `offset.hpp`, con
   due funzioni: `offsetCurve` per una curva e `offsetPolyline` per una
   polilinea.
2. **La distanza ha il segno del lato:** positiva a sinistra, negativa a
   destra, guardando nella direzione della curva (`side()`); per un cerchio e
   per un arco antiorario la sinistra è l'interno. Con distanza zero
   `offsetCurve` dà `done` con la curva stessa e gli stessi bit: l'ingresso è
   già un dato valido, anche quando la sua estensione geometrica esce dal
   quadrato delle coordinate; `offsetPolyline` dà `failed` con
   `meetsOriginal`, perché il risultato sarebbe la polilinea di partenza (punto
   5), come per una distanza così piccola che il risultato arrotondato la
   incontra.
3. **Una curva** si sposta sempre, e il risultato è dello stesso tipo: un
   segmento lungo la normale; un cerchio e un arco cambiano solo il raggio
   (l'arco tiene angolo iniziale e ampiezza, quindi le stesse direzioni esatte
   D0 e D1 dell'ADR 0010); un segmento di polilinea curvo sposta gli estremi
   lungo il raggio e prende il bulge dell'arco tra gli estremi arrotondati con
   il centro più vicino a quello esatto (il punto del loro asse più vicino al
   centro esatto, regola esatta scritta in `offset.hpp`): l'angolo al centro può
   cambiare quanto l'arrotondamento degli estremi (corretto il 2026-11-01; prima
   teneva il bulge). Se il nuovo raggio, confrontato esattamente con
   zero, è zero o negativo, la risposta è `vanished`. Ogni vertice restituito
   (anche di pezzi dritti, tagliati, prolungati o di giunzione) si arrotonda
   coordinata per coordinata al double più vicino, pareggio al pari (#132).
   Bulge e controllo di rappresentabilità usano questi double. Se gli estremi
   arrotondati di un arco non nullo coincidono, `offsetCurve` dà `failed` e
   `offsetPolyline` dà `failed` con `notRepresentable`: non lo elimina come
   pezzo puntiforme. Un segmento o pezzo dritto con estremi arrotondati
   coincidenti dà `failed` in `offsetCurve` (#148). In `offsetPolyline` si
   scarta dopo l'arrotondamento e prima dei bulge e dei controlli finali,
   togliendone anche la voce in `origins` e rendendo `simple` falso. I vicini
   hanno già lo stesso vertice double. Se questo scarto non lascia pezzi,
   `failed` con `notRepresentable`, non `vanished`: il risultato esatto
   esiste. Per una polilinea, se
   nessun pezzo resta dopo i passi 1, 3 e 4 (tutti i pezzi sono spariti, tolti o
   ridotti a un punto), la risposta è `vanished`.
4. **Una polilinea** dà il risultato definito dai quattro passi di `offset.hpp`,
   sui valori esatti: spostamento dei pezzi, giunzioni nel punto comune più
   vicino al vertice o con un arco di raggio |`d`| centrato nel vertice, pezzi
   girati al contrario tolti, pezzi ridotti a un punto tolti. Le decisioni dei
   passi (dove si incontrano due curve, quale punto è più vicino al vertice,
   se un pezzo gira al contrario) sono esatte. Due curve prolungate si
   incontrano solo se hanno punti comuni isolati: due curve sovrapposte (la
   stessa retta o lo stesso cerchio) non si incontrano, quindi la giunzione tra
   pezzi non consecutivi dà `openJoint`; in una polilinea chiusa dà `openJoint`
   anche quando resta un solo pezzo. Per un arco la giunzione si raggiunge con
   la rotazione più breve dall'estremo spostato; a mezzo giro esatto o con
   ampiezza di un giro intero o più dà `failed` con `selfIntersection`. Togliere
   un pezzo non ripristina l'estremo che una giunzione ha dato al suo vicino: un
   estremo rimasto senza vicino, il capo libero di una polilinea aperta,
   conserva il taglio; solo un estremo mai tagliato resta lo spostamento del
   passo 1. Se più pezzi girano al contrario nello stesso ciclo, si tolgono
   tutti insieme prima di rinnovare le giunzioni. I pezzi ridotti esattamente a
   un punto al passo 4 si tolgono e contano come pezzo eliminato (`simple`
   falso). L'arco di giunzione gira come gira l'ingresso nel vertice, secondo le
   tangenti dei due pezzi di partenza (se i pezzi sono resi consecutivi da
   vertici ripetuti eliminati, l'arco fa perno sul vertice alla fine del pezzo
   di partenza di A); se le tangenti sono esattamente opposte il giro non ha
   segno, e l'arco è la semicirconferenza che continua in tangenza i pezzi
   spostati: oraria per `d` positiva, antioraria per `d` negativa. L'altra
   ripercorrerebbe la polilinea di partenza, e di norma darebbe `meetsOriginal`.
5. **Mai un risultato sbagliato in silenzio.** Prima di restituire `done`,
   `offsetPolyline` controlla prima il dominio del risultato finale (punto 9,
   `beyondDomain`), poi sui `double` del risultato che due pezzi non
   consecutivi non abbiano punti comuni (esatto, come `intersect()`), che due
   pezzi consecutivi si incontrino solo nel vertice comune o in punti che
   coincidono con esso per la regola dell'ADR 0009, e che il risultato non
   incontri la polilinea di partenza. Altrimenti risponde `failed`, con il
   motivo (`OffsetFailure`): i controlli finali seguono l'ordine elencato e
   decide il primo che fallisce; non si cercano intersezioni su dati fuori
   dominio. Prima ancora, mentre compone il risultato, ogni
   arco (pezzo di partenza, tagliato o no, e arco di giunzione) prende il bulge
   con la regola del punto 3 attorno al centro esatto del suo cerchio; se
   nessun bulge lo rappresenta entro `kRelativeOffsetError`, la risposta è
   `failed` con `notRepresentable` (corretto il 2026-11-01).
6. **Che cosa è garantito**, la risposta alla domanda di Tecnigrafo:
   - linee, archi e cerchi: `done` o `vanished`, salvo `failed` per un pezzo
     dritto con estremi arrotondati coincidenti; un segmento di
     polilinea curvo anche `failed`, solo quando i suoi estremi arrotondati
     girano la corda troppo perché un bulge lo rappresenti entro l'errore (un
     arco vicino al giro intero con una corda di pochi ulp; mai se l'arco della
     regola sta entro metà dell'errore, corretto il 2026-11-01); anche quando
     gli estremi arrotondati di un arco non nullo coincidono dà `failed`;
   - polilinee nei **casi semplici** (nessun pezzo si annulla, si toglie o si
     riduce a un punto, neppure per arrotondamento, e il risultato esatto resta più lontano di epsilon(M),
     ADR 0009, da se stesso e dalla polilinea di partenza, e il risultato
     esatto e il punto m ricostruito dai double restituiti sono nel dominio): sempre `done`, con
     `simple` vero, e il risultato è quello esatto entro gli errori del punto 7,
     oppure `failed` con `notRepresentable` alla stessa condizione;
   - polilinee negli **altri casi**: `done` con il risultato dei passi, senza
     autointersezioni e senza incontrare la polilinea di partenza, oppure
     `failed`. Può differire da AutoCAD.
7. **Errori dichiarati.** I vertici entro `kRelativePointError` (2^-48) ·
   max(M, |p|, 2^-1022) dai vertici esatti; ogni punto del risultato entro
   `kRelativeOffsetError` = 2^-46 · max(M, |p|, 2^-1022) dalla curva esatta, e il
   contrario. 2^-46 è 16 volte sotto la tolleranza relativa dell'ADR 0009. M è
   la grandezza dei dati e di |`d`|. Il pavimento 2^-1022 è generale in
   `curves.hpp` (#139, #140): punti entro 2^-1070 e curve entro 2^-1068
   nei subnormali, senza cambiare le decisioni esatte.
8. **Le origini dei pezzi.** Il risultato dice per ogni pezzo da quale pezzo
   di partenza viene, o attorno a quale vertice gira se è un arco di giunzione
   (`OffsetPiece`; con pezzi resi consecutivi da vertici ripetuti eliminati, il
   vertice alla fine del pezzo di partenza di A), così Tecnigrafo conserva le
   larghezze dei pezzi che restano.
9. **Dominio e diagnosi** come nell'ADR 0010; una distanza non finita è
   `invalidValue`, una oltre `kMaxCoordinate` è `outOfDomain` (dato 1).
   `offsetCurve` dà inoltre `outOfDomain` sul dato 1 se il risultato non è
   nel dominio di `curves.hpp`: coordinate, raggio o punto m ricostruito oltre
   `kMaxCoordinate`, mai il solo sconfinamento geometrico (#143, #144).
   In `offsetCurve` il controllo di estremi e raggio esatti precede `failed`,
   anche con estremi arrotondati coincidenti o bulge non rappresentabile:
   `outOfDomain` sul dato 1 prevale; `failed` vale solo con quei valori esatti
   nel dominio. Il punto m si controlla solo per una curva che si restituisce.
   `offsetPolyline`, invece, scarta prima i dritti collassati e controlla solo
   il risultato finale (#153). Il dominio esatto riguarda gli estremi dei
   pezzi rimasti dopo i passi 1–4 e quello scarto: a un vertice condiviso si
   controllano sia la fine esatta del precedente sia l'inizio esatto del
   successivo; estremi solo di pezzi scartati non contano (#154). Il punto m
   resta ricostruito dai double restituiti; un valore oltre il dominio dà
   `failed` con `beyondDomain`
   (#138). Coordinate e raggi costruiti si controllano sui valori esatti;
   un valore esatto entro K non si restituisce oltre K, limitando a ±K
   l'arrotondamento se serve. Il punto m si ricostruisce esattamente dai
   double restituiti (#149). Il controllo del dominio riguarda solo i dati,
   non impone una regola generale sui risultati degeneri. Gli intermedi sono esatti: un pezzo poi tagliato o rimosso non
   conta, e non si eredita la diagnosi dei risultati di `offsetCurve`.
   L'ordine delle diagnosi di ingresso è quello generale di `curves.hpp`
   (#141): polilinea pezzo per pezzo, prima della distanza; i pezzi puntiformi
   si scartano e nessun pezzo rimasto dà `degenerateCurve` sul dato 0 prima
   della distanza. Tutti i controlli dei dati precedono le costruzioni (#150).
   Un arco che
   nessun bulge rappresenta dà `failed` con `notRepresentable` (nuovo valore di
   `OffsetFailure`, aggiunto in fondo il 2026-11-01). I pezzi della
   polilinea ridotti esattamente a un punto (vertici ripetuti) si tolgono senza
   diagnosi. I codici di diagnosi non cambiano.
10. **Determinismo:** la regola dell'ADR 0008, punto 5.
11. **Prestazioni**, obiettivo da misurare nella tappa: il controllo del punto 5
    confronta ogni coppia di pezzi; per le polilinee dei disegni (fino a
    qualche centinaio di pezzi) resta sotto il tempo di un comando. Una
    struttura che riduca le coppie, se servirà, non cambia il contratto.
12. **Obiettivi raggiungibili adottati.** Si garantisce il punto 6. **Non** si
    garantiscono: un risultato negli altri casi, né che coincida con quello di
    AutoCAD; le giunzioni di `OFFSETGAPTYPE` 1 e 2 (archi e smussi). Restano a
    Tecnigrafo il lato da un punto, le larghezze, i layer e il comando.

## Contratto

Intestazione pubblica in bozza in questa pull request ([ADR
0007](0007-tappe-su-richiesta.md), punto 15):

- `geom2d/include/solid_space/geom2d/offset.hpp`: `kRelativeOffsetError`,
  `OffsetOutcome`, `CurveOffset`, `offsetCurve`, `OffsetFailure`,
  `PieceOrigin`, `OffsetPiece`, `PolylineOffset`, `offsetPolyline`.

Usa senza cambiarli i tipi del contratto dell'ADR 0010 (`curves.hpp`,
`diagnosis.hpp`) e `TolerancePolicy` dell'ADR 0009. Le funzioni sono solo
dichiarate; il test `tests/geom2d/contract_test.cpp` controlla che
l'intestazione compili con le firme dichiarate, senza chiamare nulla di non
realizzato.

## Conseguenze

- Componente: `geom2d`. Nessuna nuova dipendenza.
- **Per Tecnigrafo** (lo decide il responsabile): il punto 9 della bozza
  dell'ADR 0035 ("che cosa si garantisce") può citare il punto 6 di questo ADR,
  con la definizione di caso semplice scritta qui, che aggiunge la distanza di
  epsilon(M); la scheda del pacchetto della curva parallela (P17 nella bozza
  della #330) si scrive su `offset.hpp`, con il lato tradotto in segno da
  `side()` e i tre esiti `done`, `vanished`, `failed` che corrispondono già ai
  suoi `Done`, `Vanished`, `Failed`. La verità geometrica dei casi sta in
  `tests/corpus/` (ADR 0007, punto 17).
- Rischio: le giunzioni tra curve spostate e il controllo finale sono il pezzo
  più grande della tappa. Dipende dalle intersezioni dell'ADR 0010 e dalla
  regola dell'ADR 0009, quindi viene dopo i loro pacchetti, con un pacchetto
  per le curve singole e uno per le polilinee, ciascuno con i suoi riferimenti.

## Verifica

- **Casi di riferimento** in `tests/corpus/casi/geom2d/`, scritti prima da un
  agente diverso da chi scrive il codice (ADR 0004):
  - i sei casi della richiesta, con le risposte della tabella sopra, alle due
    scale, con i dati in `double` come Tecnigrafo li darebbe;
  - i casi a–h di `autocad-offset.dxf`, che Tecnigrafo porta nell'archivio con
    la sua provenienza (ADR 0007, punto 17); il caso g con distanza 5 ha come
    atteso `failed` con `openJoint`, e la sua descrizione dice che AutoCAD
    disegna due archi (valutazione, «Su che cosa si basa»);
  - i casi limite: distanza esattamente uguale al raggio (`vanished`) e un ulp
    sotto; pezzi tangenti nei vertici, aperti e chiusi; vertici ripetuti;
    inversione di 180° (giunzione ad arco, con il verso del punto 4:
    `[(0,0),(10,0),(0,0)]` con `d` = 1 dà la semicirconferenza oraria, bulge −1,
    e con `d` = −1 quella antioraria); polilinea di un solo pezzo; distanza zero
    (`failed` con `meetsOriginal`); giunzione oltre il dominio; un caso `failed`
    per ciascun motivo.
- **Valori attesi** da aritmetica esatta: le decisioni con `fractions` di Python
  e confronti tra numeri algebrici; vertici e curve con il modulo `decimal` ad
  alta precisione, con la tolleranza del caso pari all'errore dichiarato. Per i
  casi di AutoCAD, i valori del DXF, con la tolleranza della precisione del
  file.
- **Proprietà**, provate dai test del pacchetto su dati generati in modo
  deterministico: ogni `done` supera il controllo del punto 5 rifatto con
  `intersect()`; ogni vertice di una giunzione non ad arco dista |`d`| dalle
  rette o dai cerchi dei due pezzi di partenza entro l'errore dichiarato;
  `offsetPolyline` di una polilinea di un pezzo concorda con `offsetCurve`;
  spostare di `d` e poi di −`d` un caso semplice senza giunzioni restituisce la
  polilinea di partenza entro il doppio dell'errore.
- **Determinismo:** il job della CI dell'ADR 0008, punto 5, confronta anche
  questi risultati sui tre sistemi operativi.
- **Criteri di chiusura**, dal "quando è soddisfatta" della richiesta:
  - [ ] nei casi semplici il risultato è sempre `done`, entro l'errore
    dichiarato e senza autointersezioni;
  - [ ] negli altri casi c'è un risultato senza autointersezioni o `failed`
    con il motivo, mai un risultato sbagliato in silenzio;
  - [ ] i casi della richiesta danno la risposta giusta o `failed`, con gli
    stessi bit sui tre sistemi operativi; il responsabile lo verifica anche dal
    lato di Tecnigrafo.
