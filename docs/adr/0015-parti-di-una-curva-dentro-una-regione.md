# ADR 0015: Parti di una curva dentro una regione del piano con le sue isole

- **Stato:** Accettato
- **Data:** 2026-11-11
- **Richiesta:** #215 — da Tecnigrafo: tappa 0.8, Matleyx78/tecnigrafo#661
- **Esito:** 3 pezzo nostro
- **Tappa:** 0.3, per Tecnigrafo 0.8 (scheda da scrivere all'apertura)

## Richiesta

Un retino a motivo (`ANSI31`, `NET`, i motivi dei retini letti dai file) si
disegna con famiglie di righe tagliate sul contorno: restano i tratti dentro la
regione e fuori dalle isole. Tecnigrafo conosce il contorno e le righe del
motivo, ma non sa decidere in modo coerente quali pezzi di una riga stanno
dentro quando la riga passa per un vertice, è tangente a un arco o corre lungo
un lato. Vuole gli stessi tratti sui tre sistemi, senza OCCT, con centinaia di
righe per retino e decine di retini da ridisegnare. Il testo completo è nella
issue #215; in Tecnigrafo il bisogno nasce dall'ADR 0048, punti 3 e 5.

## Valutazione

- **Spetta a solid-space.** Quali tratti di una curva stanno dentro una
  regione è una decisione geometrica del piano, compito di `geom2d`
  ([ADR 0008](0008-componente-geom2d.md)); l'ADR 0048 di Tecnigrafo, punto 5,
  la affida qui e tiene per sé il motivo (angoli, passi, origini, tratteggi),
  le unità, la traduzione delle entità e il disegno.
- **Esiti 1 e 2 esclusi da un vincolo già preso:** `core` di Tecnigrafo compila
  senza OCCT (ADR 0030 di Tecnigrafo, punto 5;
  [ADR 0007](0007-tappe-su-richiesta.md), punto 8).
- **Esito 3**, in `geom2d`, sopra le decisioni esatte dell'[ADR
  0010](0010-posizione-e-intersezioni-nel-piano.md) (dove una curva incontra le
  altre, con il tipo di contatto, i tratti in comune, le posizioni lungo la
  curva, il lato) e sui tipi delle regioni dell'[ADR
  0014](0014-contorno-e-isole-di-una-regione.md) (`region.hpp`: una regione è
  una lista di loop con la regola pari-dispari). Nessun componente nuovo,
  nessuna dipendenza nuova, nessun codice di diagnosi nuovo.
- **Su che cosa si basa:** il testo della richiesta e l'analisi dei suoi casi
  (tabella sotto); l'ADR 0048 di Tecnigrafo (righe del motivo infinite nel
  piano, tratti misurati dall'origine della famiglia, contorno non disegnato);
  la tecnica nota del taglio di una curva su una regione pari-dispari (la
  curva divisa nei punti in cui incontra i bordi, ogni parte classificata una
  volta).
- **Non verificato:** nessun prototipo eseguito, quindi le prestazioni sono un
  obiettivo da misurare nella tappa. Non è stato provato che cosa fa `HATCH` di
  AutoCAD con una riga del motivo che corre esattamente su un lato del
  contorno: il contratto restituisce quei tratti a parte, e la scelta di
  disegnarli resta a Tecnigrafo (Conseguenze).

### Che cosa rende la risposta coerente

Le difficoltà della richiesta (vertici, tangenze, righe lungo un lato) nascono
quando la decisione «dentro o fuori» si prende su un punto calcolato, per
esempio il punto medio fra due tagli arrotondati, o quando la parità si conta
sommando gli incroci senza distinguere un incrocio da un contatto. Qui nessuna
decisione passa da un valore arrotondato:

1. **I tagli sono esatti.** Dove la riga incontra i loop, e se li attraversa, li
   tocca o corre lungo un loro pezzo, lo decide l'ADR 0010 sui dati come sono.
   I tagli dividono la riga in parti aperte di lunghezza positiva, e ogni parte
   o corre lungo i loop o non ne tocca nessun punto.
2. **Ogni parte ha una classe esatta.** Una parte fuori dai loop è *dentro* o
   *fuori* secondo la regola pari-dispari della regione; una parte lungo i loop
   si classifica guardando la regione ai suoi due lati: dentro da entrambi i
   lati (due loop che si annullano lungo un tratto comune), sul *bordo* da un
   lato solo, fuori da nessuno.
3. **I tratti uniscono le parti.** Un tratto dentro è una sequenza di parti
   dentro, con i tagli fra loro: la riga che passa per un vertice o tocca un
   arco restando dentro non si spezza, e non manca nulla. Un punto in cui la
   riga tocca soltanto la regione, fuori da entrambi i lati, non è un tratto.

Solo gli estremi dei tratti si arrotondano, entro gli errori già dichiarati in
`curves.hpp` (`kRelativePointError`, `kRelativeStationError`). Le decisioni non
li leggono mai, quindi nessun esito cade in una fascia (ADR 0013, punto 4).

### I casi della richiesta

| Caso | Risposta del contratto |
|---|---|
| 1. Rettangolo con un cerchio come isola, motivo `ANSI31` | ogni riga dà i tratti fra il rettangolo e il cerchio; una riga che non incontra il cerchio, un solo tratto |
| 2a. Riga per un vertice convesso del contorno, da fuori | il vertice è un punto di solo contatto: nessun tratto |
| 2b. Riga che entra o esce per un vertice | il tratto comincia o finisce esattamente nel vertice |
| 2c. Riga per un vertice concavo, dentro da entrambi i lati | un solo tratto, che passa per il vertice |
| 2d. Riga tangente a un arco del contorno o di un'isola, dentro da entrambi i lati | un solo tratto, che passa per il punto di tangenza |
| 2e. Riga tangente da fuori | punto di solo contatto: nessun tratto |
| 3. Riga che corre sopra un lato | tratto sul bordo (`boundary`), con il lato in cui sta la regione; i tratti dentro prima e dopo finiscono e cominciano agli estremi del lato |
| 4. Isole annidate | regola pari-dispari: dentro l'isola di un'isola i tratti tornano; con gli stili *esterno* e *ignora* Tecnigrafo passa solo i loop delle profondità che servono (ADR 0014) |
| 5. Contorno con archi (polilinea con bulge) | decisioni esatte sugli archi come li definisce `curves.hpp` |
| 6. Retino `SOLID` | nessun taglio: la regione da riempire sono i loop stessi, scelti per profondità, con la regola pari-dispari (ADR 0014; ADR 0048 di Tecnigrafo, punto 3). Nessuna funzione nuova |
| 7. `autocad-retini.dxf` | casi dell'archivio, scritti dal pacchetto di riferimenti della tappa |

## Alternative considerate

- **Esito 1 o 2 (OCCT):** esclusi dall'ADR 0030 di Tecnigrafo, punto 5.
- **Esito 4:** il bisogno spetta a solid-space (sopra; ADR 0048 di Tecnigrafo,
  punto 5).
- **Nessuna funzione nuova: Tecnigrafo usa `intersectAlong` e conta la
  parità.** `intersectAlong` dà già i tagli esatti, ma la classe di ogni parte
  (vertici dove due pezzi stanno dalla stessa parte o da parti opposte,
  contatti di tangenza, tratti comuni a più loop) è proprio la decisione che la
  richiesta chiede, e Tecnigrafo la prenderebbe su valori arrotondati.
- **Una libreria esistente.** Clipper2 lavora su poligoni a coordinate intere:
  gli archi andrebbero spezzati in segmenti e le decisioni non sarebbero più
  esatte. L'arrangiamento di CGAL porta Boost e GMP, esclusi da `core` di
  Tecnigrafo (come per gli ADR 0010 e 0014).
- **Una tolleranza (ADR 0009)** per decidere che una riga «corre su un lato»
  anche quando ne dista un arrotondamento: aggiunge una fascia larga epsilon
  in cui la classe dipende dalla tolleranza, e regioni più sottili di epsilon
  da trattare a parte. Senza tolleranza, la differenza riguarda tratti che
  stanno entro un arrotondamento dal lato: il contratto la dichiara come
  limite.
- **Una funzione per una famiglia di righe** (origine, direzione, passo): sposta
  in solid-space una parte del motivo, che la richiesta lascia a Tecnigrafo.
  Una riga intera (`Extension::extended`) passata dall'origine della famiglia
  dà già le stazioni da cui Tecnigrafo misura i tratteggi.
- **Una riga per chiamata:** ogni chiamata dovrebbe preparare di nuovo la
  regione. La chiamata prende tutte le righe di un retino, così la
  realizzazione indicizza i pezzi dei loop una volta sola; il risultato di ogni
  riga resta quello che avrebbe da sola.
- **Solo i tratti dentro, con una regola fissa per quelli sul bordo:** se
  disegnare una riga che corre su un lato del contorno, che non si disegna
  (ADR 0048 di Tecnigrafo, punto 1), è una scelta di disegno, quindi di
  Tecnigrafo. Il contratto dà i due elenchi.
- **Il punto medio fra due tagli per decidere dentro o fuori:** semplice, ma
  arrotondato; vicino a un vertice o a una tangenza la risposta cambierebbe da
  un sistema all'altro, cioè il difetto che la richiesta vuole togliere.

## Decisione

1. **Una funzione in `geom2d`**, in `clipping.hpp`: `clipToRegion`, i tratti di
   ogni curva data (l'*ospite*) che stanno dentro una regione, data come lista
   di `Loop` (`region.hpp`) con la regola pari-dispari. Esatta, senza
   tolleranza.
2. **Gli ospiti** sono `Curve` qualunque, prese come sono o, con
   `Extension::extended`, una curva diritta come la sua retta intera; una curva
   ad arco resta com'è. Così Tecnigrafo passa le righe infinite del motivo
   senza calcolarne gli estremi, e misura i tratteggi dalle stazioni.
3. **Il risultato è definito da tre passi esatti**, scritti nell'intestazione:
   tagli, classi delle parti (dentro, bordo con il suo lato, fuori), tratti
   come sequenze più lunghe di parti della stessa classe. Su un ospite chiuso
   (cerchio, arco di un giro intero) i tratti non passano per il suo inizio.
4. **Due elenchi per ospite**, nell'ordine esatto lungo l'ospite: i tratti
   dentro e i tratti sul bordo, questi con il lato (`Side`, come `side()`) in
   cui sta la regione. I punti di solo contatto non sono tratti.
5. **Gli estremi dei tratti** portano il punto e la posizione sull'ospite
   (`CurvePosition`, come `intersect()`): la posizione esatta, la stazione
   entro `kRelativeStationError`, il punto entro `kRelativePointError`; gli
   estremi dei dati dell'ospite si copiano, quelli di un arco sono i punti di
   `endpoints()`. Nessuna costante nuova.
6. **Dati non validi:** un ospite o un loop non valido si scarta e si elenca
   (`SkippedInput`), e la chiamata continua; i loop si controllano come in
   `nestContours`. Nessuna diagnosi per la chiamata intera e nessun codice
   nuovo. Senza loop validi la regione è vuota.
7. **Indipendenza:** ogni ospite dà lo stesso risultato, bit per bit, che
   avrebbe da solo; le decisioni non dipendono dall'ordine dei loop, dal loro
   verso né dal vertice da cui comincia una polilinea.
8. **Determinismo:** la regola dell'ADR 0008, punto 5. Stessi dati, stessi bit
   sui tre sistemi.
9. **Prestazioni**, obiettivo da misurare nella tappa: un retino di qualche
   centinaio di righe su una regione di qualche centinaio di pezzi in pochi
   millisecondi. La realizzazione può indicizzare i pezzi dei loop (ingombri,
   ordinamento lungo la normale delle righe parallele), purché il risultato non
   cambi. La misura, con il banco di prova, entra nelle note di rilascio, come
   per gli ADR 0010 e 0014.
10. **Obiettivi raggiungibili adottati.** Si garantiscono le decisioni esatte e
    gli errori dichiarati. **Non** si garantisce, e il contratto lo scrive come
    limite:
    - che una riga calcolata a un arrotondamento da un lato risulti «sul
      bordo»: è dentro, fuori o sul bordo come dicono i suoi `double`;
    - che loop arrotondati che nei dati si toccavano (i loop di
      `detectBoundary`) non si attraversino: danno allora tratti più corti dei
      loro errori;
    - ospiti o loop con lati d'ellisse o spline (l'ellisse è la richiesta
      #217; i retini con lati d'ellisse restano non gestiti in Tecnigrafo, ADR
      0048, punto 1).

## Contratto

Intestazione pubblica in bozza in questa pull request (ADR 0007, punto 15):

- `geom2d/include/solid_space/geom2d/clipping.hpp`: `StretchEnd` (punto e
  posizione sull'ospite), `HostStretch`, `BoundaryStretch` (con `regionSide`),
  `ClippedHost`, `RegionClipping`, `clipToRegion`. L'intestazione scrive la
  regione, gli ospiti e la loro estensione, i tre passi del risultato esatto,
  gli errori degli estremi (gli estremi dei dati copiati, i punti
  d'incontro entro `kRelativePointError`, le stazioni entro
  `kRelativeStationError`, con la loro M), l'ordine dei tratti, gli scarti,
  l'indipendenza e i limiti.

Usa i tipi già dichiarati: `Curve`, `CurvePosition`, `Extension` e `Side`
dell'ADR 0010; `Loop` e `SkippedInput` dell'ADR 0014 (`region.hpp`, che questa
pull request presuppone unita). Le diagnosi sono quelle di `diagnosis.hpp`,
senza codici nuovi: gli ospiti e i loop scartati, con `invalidValue`,
`outOfDomain` o `degenerateCurve`, stanno in `skippedHosts` e `skippedLoops`
con `input` uguale al loro indice. Nessuna intestazione già accettata cambia.
La funzione è solo dichiarata; il test `tests/geom2d/contract_test.cpp`
controlla che l'intestazione compili con la firma dichiarata.

## Conseguenze

- Componente: `geom2d`, con un'intestazione nuova; nessuna dipendenza nuova.
- **Per Tecnigrafo** (lo decide il responsabile):
  - la facciata dei retini passa a `clipToRegion` le righe di ogni famiglia come
    `Segment` presi interi (`Extension::extended`), ciascuno dall'origine della
    sua riga, così le stazioni dei tratti sono già le distanze da cui misurare i
    tratteggi (ADR 0048 di Tecnigrafo, punto 2). Quali righe della famiglia
    servono lo sceglie Tecnigrafo con un ingombro per eccesso dei loop: una
    riga in più dà un risultato vuoto, una in meno è un tratto mancante;
  - la regione è fatta dai loop di `detectBoundary`, scelti per profondità
    secondo lo stile delle isole, o dai contorni scelti con le profondità di
    `nestContours`, o dai contorni letti dal file; i contorni a lati del DXF
    (linee e archi) Tecnigrafo li traduce in una polilinea chiusa, con il bulge
    dall'angolo degli archi (la tangente è la richiesta #216);
  - se disegnare i tratti sul bordo è una sua scelta: consiglio di disegnarli,
    perché la regione chiusa comprende il suo bordo e la famiglia di righe resta
    regolare anche dove una riga cade su un lato; il comportamento di AutoCAD
    non è stato provato;
  - il taglio è nelle coordinate del disegno e non dipende dalla vista: basta
    ricalcolarlo quando cambia il retino, non a ogni cambio di vista.
- **Per la richiesta #214:** nessun cambiamento; `clipToRegion` usa i suoi tipi
  così come sono.
- **Per la richiesta #217** (ellisse): se la sua valutazione aggiunge l'ellisse
  a `Curve`, decide anche come la tratta `clipToRegion` come ospite, con la
  revisione del contratto.
- **Rischi.** La classe di una parte lungo i loop e lo stato ai tagli dove più
  pezzi passano per lo stesso punto, con tangenze e curvature uguali, chiedono
  l'ordine esatto dei pezzi intorno a un punto, la stessa parte delicata
  dell'arrangiamento dell'ADR 0014: i due pacchetti possono condividerla. Lo
  stato all'inizio di un ospite preso com'è, che può cominciare dentro la
  regione, chiede una decisione esatta di appartenenza (anche per l'estremo di
  un arco, che non è un punto in `double`). Le prestazioni con molte righe
  chiedono di non provare ogni riga contro ogni pezzo.
- **Pacchetti, in via indicativa** (il piano lo scrive il responsabile
  all'apertura): `clipToRegion`, con i suoi riferimenti di fascia forte, dopo
  la revisione del contratto `[0.3-CS]`; se il piano lo ritiene utile, l'ordine
  dei pezzi intorno a un punto in un pacchetto comune con l'ADR 0014.
- Fuori da questa richiesta: i motivi e i tratteggi, l'appartenenza di un punto
  alla regione per la selezione di Tecnigrafo (ADR 0048, punto 4: se serve, è
  un'altra richiesta), ellissi e spline.

## Verifica

- **Casi di riferimento** in `tests/corpus/casi/geom2d/`, scritti prima da un
  agente diverso da chi scrive il codice (ADR 0004), con dati in `double` come
  Tecnigrafo li darebbe:
  - i casi della richiesta (tabella sopra), alle scale dell'ADR 0009
    (millimetri vicino all'origine, chilometri con coordinate di 1e5), con righe
    a 0°, 45° e 90° e righe intere e prese come sono;
  - i vertici: riga per un vertice convesso e concavo, da dentro e da fuori;
    riga per un vertice in cui si toccano due loop; riga per il punto in cui un
    loop si interseca;
  - le tangenze: a un arco del contorno e di un'isola, da dentro e da fuori; a
    due archi tangenti fra loro nello stesso punto;
  - i lati: riga lungo un lato del contorno e di un'isola, con la regione dai
    due lati possibili; lungo un tratto comune a due loop (che si annullano);
    lungo un arco (ospite ad arco sullo stesso cerchio);
  - ospiti ad arco e cerchi, chiusi, interi dentro, sul bordo, con l'inizio
    dentro la regione;
  - regioni con loop che si attraversano, polilinee che si intersecano, loop
    uguali, loop non validi; regione vuota; ospiti non validi;
  - i casi di `autocad-retini.dxf`, quando Tecnigrafo lo pubblica (ADR 0007,
    punto 17).
- **Valori attesi** indipendenti dal programma: le decisioni (numero, ordine,
  classe e lato dei tratti, posizioni esatte degli estremi) con aritmetica
  esatta (`fractions` di Python, numeri della forma a + b·√d come nell'ADR
  0010); i punti e le stazioni con il modulo `decimal`, confrontati con la
  tolleranza dell'errore garantito (ADR 0013, punto 5).
- **Proprietà**, provate dai test del pacchetto su dati generati in modo
  deterministico: permutando i loop, invertendone il verso o ruotando il
  vertice iniziale di una polilinea, le decisioni non cambiano; ogni ospite dà
  gli stessi bit da solo e insieme agli altri; invertendo un `Segment` i tratti
  si invertono e i lati si scambiano; i tratti dentro e sul bordo non si
  sovrappongono; per una regione di un solo loop convesso e una riga intera, al
  più un tratto dentro.
- **Determinismo:** il job della CI che confronta i risultati sui tre sistemi
  (ADR 0008, punto 5) include questi casi.
- **Criteri di chiusura**, dal «quando è soddisfatta» della richiesta:
  - [ ] su questi casi `clipToRegion` dà gli stessi tratti, bit per bit, sui
    tre sistemi operativi, nessun tratto fuori dalla regione o dentro
    un'isola e nessun tratto spezzato o mancante dove la riga passa per un
    vertice o è tangente;
  - [ ] il responsabile verifica dal lato di Tecnigrafo che i retini a motivo,
    nella sua CI, hanno gli stessi tratti sui tre sistemi.
