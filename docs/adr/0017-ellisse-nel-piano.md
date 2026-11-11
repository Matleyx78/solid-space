# ADR 0017: Distanza, punto più vicino e intersezioni dell'ellisse e dell'arco d'ellisse

- **Stato:** Accettato
- **Data:** 2026-11-11
- **Richiesta:** #217 — da Tecnigrafo: tappa 0.8, Matleyx78/tecnigrafo#661
- **Esito:** 3 pezzo nostro
- **Tappa:** 0.3, per Tecnigrafo 0.8 (scheda da scrivere all'apertura)

## Richiesta

Dalla 0.8 Tecnigrafo ha l'entità ellisse, come l'`ELLIPSE` del DXF (ADR 0052
di Tecnigrafo). Per sceglierla con un clic e con la finestra che attraversa, e
per gli snap *Vicino* e *Intersezione*, gli servono la distanza di un punto
dall'ellisse o dall'arco d'ellisse e il punto più vicino, le intersezioni con
segmenti, archi, cerchi e altre ellissi (anche prolungati) e se un'ellisse
attraversa un rettangolo. Gli stessi risultati sui tre sistemi, senza OCCT, con
le tangenze riconosciute come un punto solo, coerenti con quanto già vale per
archi e cerchi. `TRIM`, `EXTEND`, `OFFSET` e `FILLET` sulle ellissi non fanno
parte della richiesta. Il testo completo è nella issue #217; blocca il
pacchetto PT50 di Tecnigrafo.

## Valutazione

- **Spetta a solid-space.** Dove un'ellisse incontra un'altra curva, se la
  tocca, quale suo punto è il più vicino e se passa da un rettangolo sono
  decisioni e costruzioni geometriche del piano, compito di `geom2d`
  ([ADR 0008](0008-componente-geom2d.md)). L'ADR 0052 di Tecnigrafo, punto 4,
  le affida qui e tiene per sé l'entità, il DXF, le trasformazioni affini
  (costruzioni), l'ingombro, il disegno e i comandi.
- **Esiti 1 e 2 esclusi da un vincolo già preso:** `core` di Tecnigrafo compila
  senza OCCT (ADR 0030 di Tecnigrafo, punto 5;
  [ADR 0007](0007-tappe-su-richiesta.md), punto 8).
- **Esito 3**, in `geom2d`, con il significato esatto delle curve e le
  decisioni esatte dell'[ADR 0010](0010-posizione-e-intersezioni-nel-piano.md),
  e gli errori dichiarati dell'[ADR 0013](0013-esatto-dove-serve.md). Nessun
  componente nuovo e nessuna dipendenza nuova; uno strumento interno nuovo per
  le radici dei polinomi di quarto grado (sotto).
- **Un vincolo già preso decide l'esattezza.** L'ADR 0013, punto 1, tiene
  esatte le decisioni di `geom2d`: se e quante volte due curve si incontrano,
  la tangenza, la posizione lungo una curva. Vale anche per l'ellisse. Il
  «coerenti con archi e cerchi» della richiesta è questo: una tangenza esatta
  nei dati dà un punto solo, con `Contact::touching`; dati che erano tangenti
  prima di un arrotondamento danno la risposta esatta sui `double` ricevuti,
  come per gli archi (ADR 0010, caso 8).
- **La distanza e il punto più vicino** per segmenti e archi li calcola oggi
  Tecnigrafo da sé (ADR 0010, punto 9; `proximity.hpp` di Tecnigrafo): per
  segmenti e archi la formula è chiusa. Per l'ellisse no: i piedi delle
  perpendicolari da un punto sono le radici di un polinomio di quarto grado, e
  vicino all'evoluta due di essi si avvicinano fino a confondersi. Per questo
  la richiesta li chiede qui, e solo per l'ellisse.
- **Su che cosa si basa:** il testo della richiesta, l'ADR 0052 e la scheda
  PT48 di Tecnigrafo (piano della 0.8, Matleyx78/tecnigrafo#661: entità,
  parametri come nel DXF, rapporto in (0, 1]); la geometria delle coniche
  (l'ellisse come immagine affine del cerchio unitario, la sua equazione
  implicita, la parametrizzazione razionale); il metodo noto per la distanza
  da un'ellisse (D. Eberly, *Distance from a Point to an Ellipse, an
  Ellipsoid, or a Hyperellipsoid*, Geometric Tools, 2011), citato come
  documentazione e non come codice; i segni esatti di `exact/quadratic` e
  `exact/dyadic` della tappa 0.2.
- **Non verificato:** nessun prototipo è stato eseguito, quindi le prestazioni
  sono un obiettivo da misurare nella tappa (Decisione, punto 9). Non si è
  provato in AutoCAD che un'`ELLIPSE` con parametro iniziale uguale a quello
  finale sia l'ellisse intera, come dice il contratto: lo verifica il
  pacchetto di riferimenti con `autocad-ellissi.dxf`, e se AutoCAD fa
  diversamente si apre una issue `contratto`.

### Che cosa rende possibili decisioni esatte

L'ellisse del DXF è l'immagine del cerchio unitario sotto la mappa affine
F(x, y) = centro + x·a + y·b, con a l'asse maggiore e b = rapporto · a ruotato
di +90°. I prodotti di `double` sono numeri esatti, quindi F, la sua equazione
implicita (un polinomio di secondo grado nelle coordinate, con coefficienti
polinomiali nei dati) e la direzione di un punto sul cerchio unitario (F⁻¹,
a meno di un fattore positivo) sono esatte senza arrotondare nulla. Gli estremi
di un arco d'ellisse si definiscono come quelli di `Arc`: le direzioni
D0 = `robust::sinCos(start)` e D1 = `robust::sinCos(end)` sul cerchio unitario,
e la regola di `Arc` per le ampiezze vicine a zero e a un giro.

- **Ellisse e retta** (segmenti, segmenti di polilinea dritti, lati di un
  rettangolo): la retta sostituita nell'equazione implicita dà un polinomio di
  secondo grado. I punti sono numeri a + b·√d, come nell'ADR 0010
  (`exact/quadratic`), e la loro posizione sull'arco d'ellisse è il segno di
  un prodotto vettoriale con D0 o D1, lineare nelle coordinate.
- **Ellisse e cerchio, ellisse ed ellisse:** la parametrizzazione razionale
  della prima, sostituita nell'equazione della seconda, dà un polinomio di
  grado al più 4 con coefficienti diadici esatti. Quante radici reali ha, con
  quale molteplicità (una radice multipla è una tangenza), dove stanno rispetto
  agli estremi e in che ordine si decide con l'aritmetica esatta: parte senza
  fattori ripetuti dal massimo comun divisore con la derivata, successioni di
  Sturm, segni di polinomi nelle radici, intervalli che isolano le radici. Due
  coniche con le equazioni proporzionali sono lo stesso supporto
  (sovrapposizione). Le radici di quarto grado non stanno in generale in una
  torre di radici quadrate (`exact/tower`), quindi serve uno strumento interno
  nuovo, `exact/polynomial`, per i polinomi di grado al più 4. I punti si
  costruiscono restringendo l'intervallo della radice finché l'errore
  dichiarato è garantito.
- **Il punto più vicino:** i piedi delle perpendicolari da un punto p sono le
  intersezioni dell'ellisse con l'iperbole di Apollonio di p, cioè ancora un
  polinomio di quarto grado. Il calcolo può essere numerico (la bisezione nel
  primo quadrante del metodo di Eberly, senza trigonometria) e usare lo stesso
  strumento esatto dove due piedi si confondono. Il contratto non fissa il
  metodo: fissa che cosa si garantisce (Decisione, punto 5).
- **Il rettangolo** si decide con i soli strumenti di secondo grado: un arco
  connesso ha un punto nel rettangolo se e solo se il suo inizio ci sta o
  incontra un lato.

Come nell'ADR 0010, un filtro in virgola mobile decide i casi lontani dalle
degenerazioni e il calcolo esatto parte solo vicino a esse (punti quasi
coincidenti, curve quasi tangenti, piedi quasi coincidenti).

### I casi della richiesta

| Caso | Risposta del contratto |
|---|---|
| 1a. Ellisse di centro (0, 0), asse maggiore (10, 0), rapporto 0,5, e un segmento che la taglia | due punti `crossing`, in ordine lungo l'ellisse dal suo inizio (10, 0) in senso antiorario |
| 1b. Lo stesso, segmento da (−20, 5) a (20, 5) | un punto solo, (0, 5), `touching` |
| 1c. Lo stesso, segmento a y = 5 + 1e-12 | nessun punto (decisione esatta) |
| 2a. La stessa ellisse e il cerchio di centro (0, 0) e raggio 5 | due punti `touching`, (0, 5) e poi (0, −5) |
| 2b. Due ellissi uguali di centro (0, 0), assi (10, 0) e (0, 10), rapporto 0,5 | quattro `crossing` (±√20, ±√20), dal primo quadrante in senso antiorario lungo la prima |
| 3a. Un arco d'ellisse e una curva che incontra l'ellisse anche fuori dall'arco | con `Extension::none` solo i punti sull'arco; con l'ellisse prolungata gli altri seguono, `afterEnd` |
| 3b. Arco d'ellisse e un punto oltre i suoi estremi | `nearest` dà l'estremo più vicino, `atStart` o `atEnd`, uguale bit per bit a `endpoints` |
| 4. Ellisse di rapporto 0,01, asse maggiore (10, 0), e un punto vicino a (10, 0) | sull'asse fuori dall'ellisse, o dentro a meno di 0,001 dal vertice (il raggio di curvatura): il vertice (10, 0); sull'asse dentro, più lontano: due piedi simmetrici alla stessa distanza, e quale dei due è libero (fascia) |
| 5. Cerchio di raggio 5 sotto la scala (2, 1) e la rotazione di 30° | l'ellisse che Tecnigrafo costruisce (asse maggiore arrotondato, rapporto 0,5): le risposte esatte sui suoi `double`; una tangenza che c'era prima dell'arrotondamento può diventare due punti vicinissimi o nessuno |
| 6. Finestra che attraversa | `meetsRectangle`: vero se il rettangolo taglia l'ellisse, la contiene o la tocca su un lato; falso se è dentro l'ellisse senza toccarla, o se la parte mancante di un arco passa dal rettangolo |
| 7. `autocad-ellissi.dxf` | casi dell'archivio, scritti dal pacchetto di riferimenti della tappa |

Le risposte con le coordinate le scrive il pacchetto di riferimenti
(Verifica).

## Alternative considerate

- **Esito 1 o 2 (OCCT):** esclusi dall'ADR 0030 di Tecnigrafo, punto 5.
- **Esito 4:** il bisogno spetta a solid-space (sopra; ADR 0052 di Tecnigrafo,
  punto 4).
- **L'ellisse dentro `Curve`.** Tecnigrafo passerebbe le ellissi alle stesse
  funzioni degli altri, ma `Curve` è nel contratto accettato: ogni funzione
  che la prende (`side`, `length`, `intersectAlong`, la curva parallela, le
  tangenze, i raccordi, `detectBoundary` dell'ADR 0014) dovrebbe dire che cosa
  fa con un'ellisse, la richiesta non lo chiede per nessuna, e la facciata di
  Tecnigrafo, che distingue le alternative di `Curve`, ne troverebbe una in
  più dove oggi non se l'aspetta. Il contratto tiene
  l'ellisse a parte, in `ellipse.hpp`, e non cambia nessuna intestazione
  accettata. Se `TRIM` e le altre modifiche sulle ellissi (idea di Tecnigrafo
  #663) lo chiederanno, la loro valutazione deciderà se portarla in `Curve`.
- **Una libreria esistente.** Le coniche di CGAL (`Arr_conic_traits_2`, con
  CORE) danno intersezioni esatte, ma portano Boost e GMP, esclusi da `core` di
  Tecnigrafo come per l'ADR 0010. Geometric Tools di Eberly (licenza Boost,
  compatibile) ha distanza e intersezioni di ellissi con un'aritmetica
  razionale propria: sarebbe una dipendenza nuova, con una decisione e un ADR,
  usa la trigonometria della libreria standard (ADR 0008, punto 5) e non dà
  il nostro contratto; copiarne il codice chiederebbe una richiesta esplicita
  (AGENTS.md). Il suo metodo per la distanza resta un riferimento.
- **Approssimare l'ellisse con archi o segmenti** e usare `intersect` com'è:
  il numero dei punti, le tangenze e il punto più vicino diventerebbero
  scelte dell'approssimazione, e cambierebbero con la sua finezza. Contraddice
  l'ADR 0013, punto 1.
- **Riconoscere le tangenze entro la tolleranza** (due punti più vicini della
  tolleranza dati come uno): la regola dell'ADR 0009 cambia i dati, mai una
  decisione, e per archi e cerchi la tangenza è esatta. Se a Tecnigrafo serve
  riunire gli snap vicinissimi, lo fa sui punti restituiti con
  `TolerancePolicy::coincident`, come per gli altri snap.
- **Il punto più vicino «esatto», con i pareggi decisi** (fra due punti alla
  stessa distanza, il primo lungo l'arco): chiederebbe di confrontare
  esattamente le distanze di due radici di quarto grado, per una scelta che
  l'utente non vede. Il contratto garantisce un candidato più vicino entro
  l'errore e lascia libera la scelta dentro la fascia (Decisione, punto 5).
- **Le posizioni come parametro o lunghezza d'arco** (una `station` lungo
  l'ellisse): la lunghezza d'arco di un'ellisse è un integrale ellittico,
  costoso da garantire entro un errore, e né lo snap né la scelta la usano.
  Le posizioni sono `Location`, esatte, e l'ordine è quello della lista. Se
  `TRIM` sulle ellissi servirà un parametro, lo aggiungerà la sua richiesta.
- **I parametri come inizio e ampiezza**, come `Arc`: Tecnigrafo dovrebbe
  calcolare l'ampiezza dai parametri del DXF, con un arrotondamento in più. Il
  contratto prende i due parametri del DXF così come sono e applica la regola
  di `Arc` all'ampiezza nominale, calcolata esattamente.

## Decisione

1. **Un'intestazione nuova in `geom2d`, `ellipse.hpp`**, con il tipo
   `Ellipse` e le funzioni `magnitude`, `endpoints`, `intersect` (con una
   curva di `curves.hpp` e con un'altra ellisse), `nearest` e
   `meetsRectangle`. L'ellisse non entra in `Curve`: nessuna intestazione
   accettata cambia.
2. **`Ellipse` è l'`ELLIPSE` del DXF:** centro, estremo dell'asse maggiore
   relativo al centro, rapporto in (0, 1], parametri iniziale e finale in
   radianti. Il significato esatto è scritto nell'intestazione: l'immagine del
   cerchio unitario sotto F, l'arco da D0 a D1 in senso antiorario, con la
   regola di `Arc` applicata all'ampiezza nominale (la differenza dei
   parametri portata in (0, `kFullTurn`]). Parametri uguali, o che differiscono
   di un multiplo esatto di `kFullTurn` (0 e 2π), danno l'ellisse intera. Il
   prolungamento è l'ellisse intera.
3. **Controllo dei dati** nell'ordine di `curves.hpp`: `invalidValue` per un
   valore non finito o un rapporto fuori da (0, 1]; `outOfDomain` per
   coordinate oltre `kMaxCoordinate` o parametri oltre `kMaxTrigArgument`;
   `degenerateCurve` per un asse maggiore nullo o un arco ridotto a una
   direzione. Nessun codice di diagnosi nuovo.
4. **Le decisioni sono esatte** (ADR 0013, punto 1): quanti punti comuni,
   `crossing`, `touching` (stessa retta tangente, cioè una radice multipla) o
   `sameCurve`, le sovrapposizioni (due ellissi con gli stessi punti, o
   un'ellisse di rapporto 1 e il suo cerchio), la `Location` di ogni punto su
   ciascun ingresso, l'ordine lungo l'ellisse dal suo inizio in senso
   antiorario, se un rettangolo è incontrato, se il punto più vicino è un
   estremo. I punti costruiti stanno entro `kRelativePointError` (2^-48); i
   punti comuni isolati oltre `kMaxCoordinate` si contano in
   `omittedBeyondDomain`, come in `intersect`.
5. **Il punto più vicino** (`nearest`) considera l'arco così com'è, mai il
   prolungamento. Restituisce un candidato, un estremo o un piede di
   perpendicolare, la cui distanza supera la minima al più di
   `kRelativeNearestError` · N, con il punto e la distanza entro lo stesso
   errore (2^-44, quattro volte sotto la tolleranza relativa dell'ADR 0009;
   N è la grandezza dell'ellisse e del punto). Fra i candidati entro quella
   soglia la scelta è libera e deterministica (fascia, ADR 0013, punto 4);
   fuori dalla fascia il candidato è determinato. Se è un estremo, il punto è
   quello di `endpoints`, bit per bit, e la `Location` lo dice.
6. **`meetsRectangle`** decide esattamente se l'arco ha un punto nel
   rettangolo chiuso con i lati paralleli agli assi (la finestra che
   attraversa, bordi compresi), anche ridotto a un segmento o a un punto, in
   accordo con `intersect` e con l'inizio esatto dell'arco.
7. **Simmetria:** fra due ellissi lo scambio degli argomenti dà gli stessi
   punti esatti e le stesse decisioni, scambiati e nell'ordine lungo l'altra;
   i punti arrotondati restano entro l'errore, non necessariamente con gli
   stessi bit.
8. **Determinismo:** la regola dell'ADR 0008, punto 5. Niente trigonometria
   della libreria standard: `robust::sinCos` per D0 e D1, nessuna altra
   funzione trigonometrica serve (la richiesta #216 non è un prerequisito).
9. **Prestazioni**, obiettivo da misurare nella tappa: con il filtro in
   virgola mobile, una coppia ellisse–curva o una chiamata a `nearest` non
   degeneri in qualche decina di microsecondi al più, così lo snap su qualche
   decina di entità vicine al cursore sta sotto il millisecondo; il percorso
   esatto parte solo vicino alle degenerazioni. La misura, con il banco di
   prova, entra nelle note di rilascio, come per l'ADR 0010.
10. **Obiettivi raggiungibili adottati.** Si garantiscono le decisioni esatte
    e gli errori dichiarati. **Non** si garantiscono, e restano fuori:
    - una tangenza che c'era solo prima di un arrotondamento dei dati (caso 5):
      la risposta è quella esatta sui `double` ricevuti;
    - quale di più punti quasi alla stessa distanza dà `nearest` (punto 5);
    - lunghezza, parametro o stazione lungo l'ellisse; `intersectAlong` con le
      ellissi; `side` e la regola delle tolleranze sulle ellissi; tangenti e
      perpendicolari a un'ellisse (snap *Tangente* e *Perpendicolare*);
    - `TRIM`, `EXTEND`, `OFFSET`, `FILLET` e le altre modifiche sulle ellissi
      (idea di Tecnigrafo #663), i retini con lati d'ellisse
      (`detectBoundary` non prende ellissi, ADR 0014): servirà un'altra
      richiesta.

## Contratto

Intestazione pubblica in bozza in questa pull request (ADR 0007, punto 15):

- `geom2d/include/solid_space/geom2d/ellipse.hpp`:
  - `Ellipse`, con il significato esatto, il controllo dei dati e la regola
    dell'ampiezza nominale;
  - `magnitude` (per gli errori e per la regola dell'ADR 0009) ed `endpoints`
    (estremi entro `kRelativePointError`, anche oltre `kMaxCoordinate`), che
    sovraccaricano quelle di `curves.hpp`;
  - `EllipseIntersectionPoint`, `EllipseOverlapEnd`, `EllipseOverlap`,
    `EllipseIntersections` e le due `intersect`, con i tipi `Extension`,
    `Location` e `Contact` di `intersection.hpp` e lo stesso significato; i
    punti entro `kRelativePointError`;
  - `kRelativeNearestError` = 2^-44, `EllipseNearest` e `nearest`, con i
    candidati, i tre errori garantiti e la fascia della scelta;
  - `meetsRectangle`.

Le diagnosi sono quelle di `diagnosis.hpp`, senza codici nuovi: per l'ellisse
`invalidValue`, `outOfDomain` o `degenerateCurve` con input 0; per la curva o
la seconda ellisse le stesse con input 1; per il punto di `nearest` e per gli
angoli del rettangolo `invalidValue` o `outOfDomain`, con input 1 (e 2 per il
secondo angolo). Il commento di `DiagnosisCode::invalidValue` in
`diagnosis.hpp` non nomina il rapporto: lo dice `ellipse.hpp`, e
l'intestazione accettata non si tocca. Le funzioni sono solo dichiarate; il
test `tests/geom2d/ellipse_contract_test.cpp`, nel bersaglio dei test del
contratto, controlla che l'intestazione compili con le firme dichiarate e che
`Curve` non cambi.

## Conseguenze

- Componente: `geom2d`, con un'intestazione nuova e uno strumento interno
  nuovo (`exact/polynomial`, radici reali di polinomi di grado al più 4 con
  segni esatti). Nessuna dipendenza nuova.
- **Per Tecnigrafo** (lo decide il responsabile, con le schede di PT48 e
  PT50):
  - la facciata traduce `core::Ellipse` in `geom2d::Ellipse` copiando i suoi
    cinque valori, dopo il ribaltamento dell'estrusione `(0,0,-1)` che fa già
    il codec (scheda PT48, punto 7); il rapporto in (0, 1] e l'asse non nullo
    di `supported()` coincidono con il controllo dei dati;
  - come per gli archi, gli estremi di un arco d'ellisse (snap *Estremo*)
    andrebbero presi da `endpoints`, non da `pointAt` con `core::sinCos`, così
    concordano con le intersezioni (oggi la scheda PT48 li calcola da sé);
  - l'ellisse non è una `Curve`: la facciata delle intersezioni ha un ramo per
    l'ellisse, e una polilinea si passa pezzo per pezzo (un vertice comune a
    due pezzi è `atEnd` su uno e `atStart` sull'altro);
  - la scelta con un clic confronta `nearest(...).distance` con l'apertura;
    la finestra che attraversa usa `meetsRectangle`, la finestra che contiene
    resta sull'ingombro di Tecnigrafo.
- **Rischi.** Lo strumento di quarto grado è la parte nuova e delicata: radici
  multiple, coefficienti diadici lunghi, il confronto di radici con gli
  estremi dell'arco. Il punto più vicino vicino all'evoluta è mal
  condizionato; il contratto lo assorbe con la fascia, ma chi lo realizza deve
  garantire l'errore anche lì. La tappa dedica allo strumento un pacchetto a
  sé, con riferimenti di fascia forte.
- **Pacchetti, in via indicativa** (il piano lo scrive il responsabile
  all'apertura), dopo la revisione del contratto `[0.3-CS]`, ognuno con i suoi
  riferimenti: `Ellipse`, `magnitude`, `endpoints`, l'ellisse con le rette e
  `meetsRectangle` (secondo grado); lo strumento di quarto grado; l'ellisse
  con cerchi, archi e altre ellissi; `nearest`.
- Fuori da questa richiesta: punto 10.

## Verifica

- **Casi di riferimento** in `tests/corpus/casi/geom2d/`, scritti prima da un
  agente diverso da chi scrive il codice (ADR 0004), con dati in `double` come
  Tecnigrafo li darebbe:
  - i casi della richiesta (tabella sopra), alle scale dell'ADR 0009
    (millimetri vicino all'origine, chilometri con coordinate di 1e5);
  - il significato: ellisse intera con parametri 0 e 2π e con parametri
    uguali, parametri con l'iniziale maggiore del finale, archi di ampiezza
    quasi zero e quasi un giro, rapporto 1 e rapporti piccoli (0,01 e
    1e-6), assi ruotati, asse nullo e dati fuori dominio;
  - le intersezioni: tangenze esatte e quasi tangenze di un ulp con rette,
    cerchi ed ellissi; quattro punti, tangenze doppie, ellissi con gli stessi
    punti date in modi diversi (asse opposto, parametri spostati di π), ellisse
    di rapporto 1 sul suo cerchio, intersezioni negli estremi degli archi,
    prolungamenti;
  - `nearest`: punti fuori, dentro, sull'ellisse, al centro, sugli assi,
    vicino all'evoluta, oltre gli estremi di un arco; dove il caso cade in una
    fascia, il riferimento scrive le proprietà che valgono per ogni scelta,
    non il punto (ADR 0013, punto 5);
  - `meetsRectangle`: rettangolo che taglia, che contiene, interno senza
    tocco, tangente a un lato, ridotto a un segmento e a un punto, sulla parte
    mancante di un arco;
  - i casi di `autocad-ellissi.dxf`, quando Tecnigrafo lo pubblica (ADR 0007,
    punto 17), compreso un arco con parametri uguali.
- **Valori attesi** indipendenti dal programma: le decisioni (numero dei punti,
  contatti, posizioni, ordine, sovrapposizioni, esito del rettangolo, estremo o
  no) con aritmetica esatta (`fractions` di Python, numeri della forma
  a + b·√d come nell'ADR 0010, e per il quarto grado radici isolate con
  successioni di Sturm in razionali); le coordinate e le distanze con il modulo
  `decimal`, confrontate con la tolleranza dell'errore garantito (ADR 0013,
  punto 5).
- **Proprietà**, provate dai test del pacchetto su dati generati in modo
  deterministico: lo scambio di due ellissi dà le stesse decisioni;
  `meetsRectangle` concorda con `intersect` sui lati e con l'inizio; la
  distanza di `nearest` non supera quella di nessun punto campionato sulla
  curva più l'errore; un punto di `nearest` su un'ellisse intera non è mai
  `atStart` né `atEnd`.
- **Determinismo:** il job della CI che confronta i risultati sui tre sistemi
  (ADR 0008, punto 5) include questi casi.
- **Criteri di chiusura**, dal «quando è soddisfatta» della richiesta:
  - [ ] su questi casi `intersect`, `nearest` e `meetsRectangle` danno gli
    stessi risultati, bit per bit, sui tre sistemi operativi, e le tangenze
    esatte sono un punto solo, `touching`;
  - [ ] il responsabile verifica dal lato di Tecnigrafo che, nella sua CI, lo
    snap *Vicino* e *Intersezione* e la scelta con un clic su un'ellisse danno
    lo stesso risultato su Linux, Windows e macOS.
