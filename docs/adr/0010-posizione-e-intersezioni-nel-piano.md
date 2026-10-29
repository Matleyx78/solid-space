# ADR 0010: Posizione reciproca e intersezioni di linee, archi e cerchi del piano

- **Stato:** Accettato
- **Data:** 2026-10-29
- **Richiesta:** #20 — da Tecnigrafo: tappa 0.7, Matleyx78/tecnigrafo#326
- **Esito:** 3 pezzo nostro
- **Tappa:** 0.2 (scheda da scrivere all'apertura)

## Richiesta

Per due curve del piano (segmenti, archi, cerchi, anche come pezzi di una
polilinea e anche prolungati) Tecnigrafo deve sapere se si intersecano, si
toccano o si sovrappongono, dove, e in che posizione lungo ciascuna curva; e da
che parte di una curva sta un punto. Su queste risposte poggiano lo snap a
intersezione, `TRIM`, `EXTEND` e, a cascata, `OFFSET`, `FILLET`, `CHAMFER` e
`JOIN`. Le risposte devono essere simmetriche, non contraddirsi mai, dare gli
stessi bit sui tre sistemi operativi ed essere abbastanza veloci per lo snap a
ogni movimento del mouse. Il testo completo è nella issue #20.

## Valutazione

- **Spetta a solid-space:** predicati robusti e curve del piano sono compiti di
  `robust` e `geom2d` ([ADR 0008](0008-componente-geom2d.md), prerequisito di
  questa valutazione).
- **Esiti 1 e 2 esclusi da un vincolo già preso:** `core` di Tecnigrafo compila
  senza OCCT (ADR 0030 di Tecnigrafo, punto 5; [ADR
  0007](0007-tappe-su-richiesta.md), punto 8). Anche senza il vincolo
  `Geom2dAPI_InterCurveCurve` non basterebbe: decide con tolleranze, che sono la
  causa delle decisioni contraddittorie descritte nella richiesta, e non dà gli
  stessi bit sui tre sistemi.
- **Esito 3**, in `geom2d` sopra `robust`. La richiesta è inseparabile dalla
  #23 ([ADR 0009](0009-regola-delle-tolleranze-del-piano.md)): stessa tappa,
  stesso contratto.
- **Su che cosa si basa:** il testo della richiesta, l'analisi dei suoi casi
  (sotto) e la tecnica nota dei predicati esatti con filtro in virgola mobile.
  **Non verificato:** nessun prototipo è stato eseguito, quindi le prestazioni
  sono un obiettivo da misurare nella tappa, non un dato.

### Che cosa rende possibili decisioni esatte

Ogni curva ha un **significato esatto fissato dai suoi `double`**
(`curves.hpp`):

- un segmento e un cerchio sono i loro dati;
- un arco di Tecnigrafo (centro, raggio, angolo iniziale, ampiezza con segno)
  è il tratto del cerchio esatto tra le direzioni D0 e D1 date dal seno e dal
  coseno **deterministici** di `robust` (ADR 0008, punto 5), con tre regole
  scritte per i casi in cui l'arrotondamento fa scavalcare D0 a D1 (ampiezza
  quasi zero o quasi un giro);
- un segmento di polilinea con bulge è l'arco del cerchio che passa per i due
  estremi e per il punto medio dell'arco, che è un razionale dei dati:
  nessuna trigonometria, nessun arrotondamento.

Con queste definizioni ogni coordinata di un'intersezione è della forma
a + b·√d, con a, b e d razionali calcolabili esattamente dai dati. Decidere un
segno, un ordine o un'uguaglianza tra numeri di questa forma, anche con due
radici diverse (punti di coppie diverse lungo la stessa curva), si fa con un
numero fisso di elevamenti al quadrato: il grado resta limitato, quindi anche
il caso peggiore ha un costo limitato. Nel caso comune un filtro in virgola
mobile decide da solo, e il calcolo esatto parte solo vicino alle degenerazioni
(punti quasi coincidenti, curve quasi tangenti).

### I casi della richiesta

| Caso | Risposta esatta |
|---|---|
| 1. Segmenti (0,0)-(10,10) e (0,10)-(10,0) | un incrocio in (5, 5), all'interno di entrambi |
| 2. Cerchio r = 5 contro y = 5; y = 5 + 1e-12; y = 5 − 1e-12 | tangenza in (0, 5); nessun punto; due incroci a x ≈ ±3.16e-6 |
| 3. Archi dello stesso cerchio sovrapposti per 30° | una sovrapposizione, con i due estremi su entrambe le curve |
| 4. Segmento che finisce su un altro (a T) | un incrocio, `atEnd` sul primo e `inside` sul secondo |
| 5. Polilinea chiusa che si interseca in un suo vertice | per i pezzi che vi arrivano, punti `atStart`/`atEnd`; per il pezzo che la attraversa, un incrocio `inside` nello stesso punto esatto |
| 6. Segmento (0,0)-(1,0) prolungato contro il cerchio (10,0) r = 2 | senza prolungamento nessun punto; con il prolungamento (8, 0) e (12, 0), `afterEnd` |
| 7. Punto a 1e-13 da un segmento | il lato esatto, sinistra o destra secondo i dati |
| 8. Le figure traslate di (1e6, 1e6) o scalate di 1e-3 | la risposta esatta per i dati traslati, che sono già arrotondati: 1e6 + 5 ± 1e-12 vale 1e6 + 5 in `double`, quindi il caso 2 traslato dà tre tangenze; scalate di 1e-3 le tre risposte del caso 2 restano diverse |

Il caso 8 mostra il limite di ogni regola scritta sui `double`: la traslazione
cambia i dati prima di qualunque decisione. La regola dell'ADR 0009 dice che
cosa l'utente intendeva; le decisioni di questo ADR dicono che cosa sono i dati.

## Alternative considerate

- **Esito 1 o 2 (OCCT):** esclusi dall'ADR 0030 di Tecnigrafo, punto 5.
- **Esito 4:** il bisogno spetta a solid-space (sopra).
- **Una libreria esistente** (per esempio il kernel circolare di CGAL, GPL, che
  fa esattamente questo): porta Boost e GMP, e `core` di Tecnigrafo non ha
  dipendenze esterne (ADR 0030 di Tecnigrafo, punti 4 e 5).
- **Decisioni con tolleranza, come oggi in `core`:** due decisioni sugli stessi
  dati possono contraddirsi, come descrive la richiesta.
- **Solo il filtro in virgola mobile, senza calcolo esatto:** sbaglia proprio
  vicino alle degenerazioni, i casi 2, 4, 5 e 7.
- **Solo calcolo esatto, senza filtro:** corretto ma troppo lento per lo snap a
  ogni movimento del mouse.
- **Solo intersezioni a coppie, con l'ordinamento lasciato a Tecnigrafo:** la
  facciata ordinerebbe stazioni arrotondate, che passano dalla trigonometria, e
  due punti esattamente coincidenti di coppie diverse potrebbero risultare
  distinti o invertiti. Per questo il contratto ha anche `intersectAlong`.
- **Un'interfaccia per le polilinee intere** (autointersezioni, punto dentro o
  fuori): non serve a questa richiesta, perché la facciata compone le risposte
  pezzo per pezzo (`pieces`). Se servirà, nascerà da una richiesta.

## Decisione

1. **Ogni curva ha un significato esatto** dato dai suoi `double`, scritto in
   `curves.hpp`, come riassunto sopra.
2. **Ogni decisione è esatta** su quel significato, senza tolleranze: quanti
   punti comuni, se le curve si incrociano, si toccano o stanno sulla stessa
   retta o sullo stesso cerchio (`Contact`); le sovrapposizioni; dove sta ogni
   punto su ciascuna curva (`Location`: prima dell'inizio, all'inizio, dentro,
   alla fine, oltre la fine); l'ordine dei punti lungo una curva; quali punti di
   coppie diverse sono lo stesso punto; il lato di un punto (`side`). Ne seguono
   la simmetria e l'assenza di contraddizioni.
3. **Le costruzioni hanno un errore dichiarato.** Un punto restituito dista dal
   punto esatto al più 2^-48 · max(M, |p|); una stazione o una lunghezza al più
   2^-46 · max(M, |s|), con M la grandezza dei dati. Le stazioni non decrescono
   mai lungo una lista ordinata. Lo scambio dei due argomenti dà gli stessi bit.
4. **Il punto arrotondato non è una decisione.** Un punto restituito non sta in
   generale esattamente sulle curve: le decisioni su un'intersezione si leggono
   dal risultato, non rifacendole sul punto arrotondato. Gli estremi di un arco
   si prendono da `endpoints`, non dal coseno e dal seno di Tecnigrafo.
5. **Dominio.** Coordinate e raggi fino a 2^50 in modulo, angoli fino a 2^20.
   Fuori dal dominio, o con dati non validi, c'è una diagnosi. Le curve
   esattamente ridotte a un punto danno la diagnosi `degenerateCurve` (la regola
   dell'ADR 0009 permette a Tecnigrafo di toglierle prima). Le intersezioni
   oltre il dominio (due rette prolungate quasi parallele) non si elencano, ma
   si contano.
6. **Due operazioni di intersezione:** `intersect`, per una coppia (snap), e
   `intersectAlong`, per una curva contro molte (`TRIM`, `EXTEND`), con un solo
   ordine esatto lungo la curva e i punti esattamente coincidenti di tagli
   diversi riuniti. Un tagliente non valido non fa fallire la chiamata: si
   scarta e si elenca.
7. **Determinismo:** la regola dell'ADR 0008, punto 5. Stessi dati, stessi bit
   con MSVC, GCC e Clang su Windows, Linux e macOS.
8. **Prestazioni**, obiettivo da misurare nella tappa: con il filtro in virgola
   mobile una coppia non degenere costa qualche microsecondo al più, così decine
   di coppie stanno sotto il millisecondo; il percorso esatto parte solo vicino
   alle degenerazioni e ha un costo limitato. Una misura con un banco di prova,
   non un test della CI, entra nelle note di rilascio.
9. **Obiettivi raggiungibili adottati.** Si garantiscono le decisioni esatte e
   gli errori dichiarati sopra. **Non** si garantiscono: l'invarianza per
   traslazione o scala (caso 8); che un punto arrotondato stia esattamente sulle
   curve (punto 4); un risultato per i dati fuori dal dominio. Restano fuori
   dalla richiesta, e a Tecnigrafo, la proiezione di un punto su una curva (che
   ha già per lo snap a perpendicolare) e le operazioni su polilinee intere.

## Contratto

Intestazioni pubbliche in bozza in questa pull request ([ADR
0007](0007-tappe-su-richiesta.md), punto 15):

- `robust/include/solid_space/robust/point.hpp`: `Point2`, `kMaxCoordinate`.
- `robust/include/solid_space/robust/predicates.hpp`: `Sign`, `orient2d`
  (esatto).
- `robust/include/solid_space/robust/trigonometry.hpp`: `sinCos`, `atan2`,
  deterministici, errore al più 2 ulp (`kTrigMaxErrorUlps`), angoli fino a
  `kMaxTrigArgument`.
- `robust/include/solid_space/robust/result.hpp`: `Result<T, D>`.
- `geom2d/include/solid_space/geom2d/diagnosis.hpp`: `DiagnosisCode`
  (`invalidValue`, `outOfDomain`, `degenerateCurve`, `notCircular`),
  `Diagnosis` (con l'indice del dato), `Result<T>`.
- `geom2d/include/solid_space/geom2d/curves.hpp`: `Segment`, `Arc`, `Circle`,
  `PolylineSegment`, `Curve`, `PolylineVertex`, `Polyline`, `pieces`,
  `magnitude`, `endpoints`, `length`; il significato esatto di ogni curva, le
  regole di controllo dei dati e gli errori garantiti `kRelativePointError`
  (2^-48) e `kRelativeStationError` (2^-46).
- `geom2d/include/solid_space/geom2d/intersection.hpp`: `Extension`,
  `Location`, `Contact`, `CurvePosition`, `IntersectionPoint`, `Overlap`,
  `PairIntersections`, `intersect`; `Cut`, `CutterContact`, `CutterOverlap`,
  `SkippedCutter`, `HostIntersections`, `intersectAlong`.
- `geom2d/include/solid_space/geom2d/side.hpp`: `Side`, `side`.

Le funzioni sono solo dichiarate; `Result`, un modello di classe, è l'unica
definizione. I test `tests/robust/contract_test.cpp` e
`tests/geom2d/contract_test.cpp` controllano che le intestazioni compilino con
le firme dichiarate, senza chiamare nulla di non realizzato.

## Conseguenze

- Componenti: `robust` (predicati, trigonometria, tipi di base) e `geom2d`
  (curve, intersezioni, lato).
- Le richieste #21 e #22 usano queste curve e queste decisioni: le loro
  valutazioni aggiungeranno a `geom2d` tangenze, raccordi e curva parallela,
  senza cambiare questo contratto se non con l'etichetta `contratto`.
- **Per Tecnigrafo** (lo decide il responsabile): la facciata traduce le entità
  in `Curve` e le polilinee con `pieces`; non riordina e non confronta da sé le
  stazioni; prende gli estremi degli archi da `endpoints`. La verità geometrica
  dei casi sta in `tests/corpus/` (ADR 0007, punto 17), e i riferimenti di
  Tecnigrafo verificano la facciata e i comandi.
- Rischio: il calcolo esatto con due radici diverse è la parte più delicata da
  scrivere e da provare. La tappa le dedica un pacchetto a sé, con riferimenti
  propri, dopo segmenti e cerchi.

## Verifica

- **Casi di riferimento** in `tests/corpus/casi/geom2d/` e
  `tests/corpus/casi/robust/`, scritti prima da un agente diverso da chi scrive
  il codice (ADR 0004):
  - gli otto casi della richiesta, con le risposte della tabella sopra, alle
    scale che la richiesta indica e con i dati in `double` come Tecnigrafo li
    darebbe;
  - per ogni coppia di tipi (segmento, arco, cerchio, segmento di polilinea) i
    casi limite: punti coincidenti, incroci agli estremi, tangenze esatte e
    quasi tangenze di un ulp, sovrapposizioni parziali, totali e ridotte a un
    punto, archi di ampiezza quasi zero e quasi un giro, prolungamenti;
  - `intersectAlong` con più tagli per lo stesso punto esatto;
  - `orient2d` su punti quasi allineati (ADR 0004, punto 9);
  - `sinCos` e `atan2` su una griglia di angoli, compresi multipli di π/4 e
    valori vicini a 2^20.
- **Valori attesi** da aritmetica esatta: i dati sono razionali, le decisioni
  si ricavano con `fractions` di Python e confronti tra numeri della forma
  a + b·√d; le coordinate attese con il modulo `decimal` ad alta precisione,
  con la tolleranza del caso pari all'errore garantito. Per la trigonometria,
  serie calcolate con `decimal`.
- **Proprietà**, provate dai test del pacchetto su dati generati in modo
  deterministico, anche quasi degeneri: simmetria bit per bit; `side` uguale a
  `on` esattamente nei punti che `intersect` dà sulla curva; stazioni non
  decrescenti; ogni tagliente di `intersectAlong` uguale alla sua chiamata a
  coppie.
- **Determinismo:** un job della CI confronta bit per bit i risultati di un
  insieme fisso di casi sui tre sistemi operativi (ADR 0008, punto 5).
- **Criteri di chiusura**, dal "quando è soddisfatta" della richiesta:
  - [ ] le risposte sono simmetriche;
  - [ ] intersezione, tangenza, sovrapposizione e lato non si contraddicono
    sugli stessi dati;
  - [ ] i punti stanno entro l'errore dichiarato e le posizioni lungo ciascuna
    curva sono in ordine coerente;
  - [ ] i casi della richiesta danno la risposta giusta, con gli stessi bit sui
    tre sistemi operativi; il responsabile lo verifica anche dal lato di
    Tecnigrafo.
