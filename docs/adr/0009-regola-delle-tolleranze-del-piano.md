# ADR 0009: Una regola unica per coincidenze e tolleranze del piano

- **Stato:** Accettato
- **Data:** 2026-10-29
- **Richiesta:** #23 — da Tecnigrafo: tappa 0.7, Matleyx78/tecnigrafo#329
- **Esito:** 3 pezzo nostro
- **Tappa:** 0.2 (scheda da scrivere all'apertura)

## Richiesta

Tecnigrafo chiede una regola unica che dica quando due punti coincidono, quando
due segmenti sono allineati, quando due archi stanno sullo stesso cerchio,
quando un segmento o un arco è degenere e quando un bulge vale zero, sensata sia
per un disegno in millimetri vicino all'origine sia per uno in chilometri con
coordinate di 1e5 o 1e6, e coerente con le decisioni della richiesta #20. Oggi
`core` ha soglie sparse (1e-9·L² per la collinearità, 1e-12 per il bulge, 32
epsilon per gli angoli), e la tappa 0.7 ne richiede di nuove per `JOIN` e
`TRIM`. Il testo completo è nella issue #23.

## Valutazione

- **Spetta a solid-space.** La politica delle tolleranze è un compito di
  `robust` ([ADR 0002](0002-componenti-e-dipendenze.md),
  [ARCHITETTURA](../ARCHITETTURA.md)), e la regola deve stare accanto ai
  predicati della richiesta #20 per esserne coerente.
- **Esiti 1 e 2 esclusi da un vincolo già preso.** `core` di Tecnigrafo compila
  senza librerie esterne, quindi senza OCCT (ADR 0030 di Tecnigrafo, punto 5;
  [ADR 0007](0007-tappe-su-richiesta.md), punto 8). Anche senza il vincolo OCCT
  non basterebbe: ha costanti globali (`Precision::Confusion`, 1e-7), non una
  regola che segua la scala del disegno.
- **Esito 3**: la regola sta in `robust` (`TolerancePolicy`), e la sua
  applicazione alle curve in `geom2d`, il componente dell'[ADR
  0008](0008-componente-geom2d.md), che è un prerequisito di questa
  valutazione.
- **Inseparabile dalla #20.** Le decisioni della #20 (intersezioni, lato,
  ordine) sono esatte e non usano tolleranze; questa regola risponde a un'altra
  domanda, che cosa intendeva l'utente. Scriverle separate significherebbe
  scrivere due volte i predicati, o due regole che non si conoscono. Per questo
  le due valutazioni hanno lo stesso contratto e vanno nella stessa tappa.
- **Su che cosa si basa:** il testo della richiesta e il calcolo a mano dei suoi
  casi con la regola proposta (tabella sotto). **Non verificato:** nessun
  prototipo è stato eseguito; se il pavimento di un nanometro vada bene per
  ogni uso di Tecnigrafo lo dirà la tappa (vedi Conseguenze).

## Alternative considerate

- **Esito 1 o 2 (OCCT):** esclusi dall'ADR 0030 di Tecnigrafo, punto 5.
- **Esito 4:** il bisogno spetta a solid-space (sopra). Restano a Tecnigrafo le
  tolleranze dello schermo (mira in pixel, distanza di snap) e la traduzione
  delle unità del DXF in metri.
- **Una tolleranza assoluta fissa** (come i 1e-10 di AutoCAD): con coordinate di
  1e6 una differenza di 1e-10 è già sotto la risoluzione dei `double` (un ulp di
  1e6 vale circa 1.2e-10), quindi la regola non distinguerebbe più nulla di
  quello che i dati possono dire.
- **Una tolleranza solo relativa alla grandezza dei dati:** vicino all'origine
  tende a zero, e (0, 0) e (1e-200, 0) risulterebbero diversi.
- **Una tolleranza relativa all'estensione dell'intero disegno:** la risposta
  per due entità dipenderebbe dal resto del disegno e cambierebbe mentre il
  disegno cresce.
- **Una tolleranza sugli angoli separata da quella sulle lunghezze:** un angolo
  piccolo su un segmento lungo è uno scostamento grande; misurare tutto come
  distanza tiene la regola coerente con la scala.
- **Le tolleranze dentro i predicati** (come fa OCCT): è proprio la causa delle
  decisioni contraddittorie che la #20 vuole eliminare.

## Decisione

1. **Due domande, due strumenti.** Le decisioni esatte dicono che cosa sono i
   dati (ADR 0010) e non usano mai una tolleranza. La regola di questo ADR dice
   che cosa intendeva l'utente; si applica solo con le funzioni che la
   nominano, e la sua risposta serve a cambiare i dati (unire due punti, togliere
   un pezzo degenere, mettere un bulge a zero) prima di nuove decisioni, mai a
   prenderle al loro posto.
2. **La scala.** Per dati di grandezza M la tolleranza è

   ```text
   epsilon(M) = max(f, r · M)      f = 1e-9 m espresso in unità del disegno
                                   r = 2^-42 ≈ 2.3e-13
   ```

   - M è il massimo valore assoluto tra le coordinate dei punti e dei centri e
     i raggi delle curve in gioco (per un segmento di polilinea curvo, anche le
     coordinate del punto medio del suo arco); dipende solo dai dati giudicati.
   - f è il pavimento fisico: un nanometro. Tecnigrafo dà la lunghezza della sua
     unità in metri, ricavata da `INSUNITS` (0, senza unità, vale millimetri come
     già fa Tecnigrafo).
   - r è 64 volte l'errore massimo di un punto costruito da `geom2d` (2^-48,
     ADR 0010): un punto costruito coincide sempre, per la regola, con il punto
     esatto che rappresenta.
3. **Una regola per ogni domanda**, tutte nella forma "una distanza esatta è al
   più epsilon":

   | Domanda | Funzione | Vero quando |
   |---|---|---|
   | Due punti coincidono | `robust::TolerancePolicy::coincident` | la distanza è al più epsilon |
   | Un pezzo è degenere | `geom2d::isDegenerate` | ogni due punti del pezzo distano al più epsilon: la lunghezza di un segmento, la corda di un arco fino a mezzo giro, il diametro di un arco più grande o di un cerchio |
   | Un bulge vale zero | `geom2d::isStraight` | l'arco si allontana dalla corda al più di epsilon (freccia \|bulge\| · corda / 2) |
   | Due segmenti sono allineati | `geom2d::areAligned` | ogni estremo di ciascuno dista al più epsilon dalla retta dell'altro |
   | Due archi sono concentrici | `geom2d::areConcentric` | i centri distano al più epsilon |
   | Due archi stanno sullo stesso cerchio | `geom2d::onSameCircle` | concentrici, e i raggi differiscono al più di epsilon |
   | Una lunghezza è trascurabile | `robust::TolerancePolicy::negligible` | la lunghezza è al più epsilon |

4. **Ogni confronto con epsilon è esatto:** la distanza esatta dei `double`
   ricevuti contro il `double` epsilon, senza arrotondamenti che decidano. La
   risposta è la stessa su ogni piattaforma.
5. **Coerenza con la richiesta #20.**
   - Dati esattamente coincidenti, allineati o degeneri lo sono anche per la
     regola: la regola allarga le risposte esatte, non le contraddice.
   - Un punto costruito da `geom2d` (un'intersezione, un estremo d'arco)
     coincide per la regola con il punto esatto, e due costruzioni dello stesso
     punto esatto coincidono tra loro.
   - La regola non è transitiva: a ~ b e b ~ c non danno a ~ c. Chi la applica a
     catene (`JOIN` su molte entità) la applica in un ordine fissato, e lo
     scrive.
6. **Fuori da questa richiesta:** le tolleranze dello schermo e la traduzione di
   `INSUNITS` restano a Tecnigrafo; il suo ADR delle tolleranze parte da qui.

I casi della richiesta, calcolati a mano con la regola:

| Caso | mm, vicino all'origine | km, coordinate di 1e5 |
|---|---|---|
| Tolleranza | f = 1e-6 mm; r · M ≈ 4.5e-12 (M = 20); epsilon = 1e-6 mm | f = 1e-12 km; r · M ≈ 2.3e-8 (M = 1e5); epsilon ≈ 2.3e-8 km (23 µm) |
| 1. Estremi (10, 0) e (10, 1e-10) | coincidono (1e-10 ≤ 1e-6) | coincidono |
| 2. Linee lunghe 10 che divergono di 1e-12 rad | allineate (scarto 1e-11) | allineate |
| 3. Archi con i centri a 1e-11 e lo stesso raggio | sullo stesso cerchio | sullo stesso cerchio |
| 5. Pezzo lungo 1e-14 | degenere | degenere |

Il caso 4 della richiesta è la seconda colonna. I valori esatti dei casi (dati
in `double`, quindi già arrotondati alla scala delle coordinate) li calcola il
pacchetto di riferimenti della tappa.

## Contratto

Intestazioni pubbliche in bozza in questa pull request ([ADR
0007](0007-tappe-su-richiesta.md), punto 15):

- `robust/include/solid_space/robust/tolerance.hpp`: `kPhysicalFloorMeters`,
  `kRelativeTolerance`, `TolerancePolicy` (`forUnit`, `unitInMeters`,
  `floorLength`, `epsilon`, `coincident`, `negligible`).
- `robust/include/solid_space/robust/result.hpp` e `diagnosis.hpp`:
  `Result<T, D>`, l'unico tipo già realizzato perché è un modello di classe;
  `DiagnosisCode::invalidValue` per un'unità non valida.
- `robust/include/solid_space/robust/point.hpp`: `Point2`, `kMaxCoordinate`.
- `geom2d/include/solid_space/geom2d/coincidence.hpp`: `isDegenerate`,
  `isStraight`, `areAligned`, `areConcentric`, `onSameCircle`. I tipi delle
  curve e le diagnosi di `geom2d` sono quelli del contratto dell'ADR 0010
  (`curves.hpp`, `diagnosis.hpp`).

Nessuna funzione costruisce valori: tutte le risposte sono esatte rispetto a
epsilon, che è un `double` calcolato in modo deterministico. Le precondizioni
violate danno un'asserzione in debug; i dati non validi delle curve danno una
diagnosi. Il test `tests/robust/contract_test.cpp` e
`tests/geom2d/contract_test.cpp` controllano che le intestazioni compilino con
le firme dichiarate.

## Conseguenze

- Componenti: `robust` (regola) e `geom2d` (applicazione alle curve).
- Tecnigrafo potrà sostituire le soglie di `core` con questa regola e scrivere
  il suo ADR delle tolleranze partendo da qui (ADR 0030 di Tecnigrafo,
  conseguenze); quali soglie e come lo decide Tecnigrafo.
- Il pavimento di un nanometro è una scelta: se un uso reale di Tecnigrafo lo
  trova troppo grande o troppo piccolo, cambiarlo è una modifica del contratto
  (etichetta `contratto`).
- Fuori dalla tappa: tolleranze in 3D, che nasceranno dalle richieste della fase
  2 di Tecnigrafo.

## Verifica

- **Casi di riferimento** in `tests/corpus/casi/geom2d/` e
  `tests/corpus/casi/robust/`, scritti prima da un agente diverso da chi scrive
  il codice (ADR 0004):
  - i cinque casi della richiesta, alle due scale (millimetri vicino
    all'origine; chilometri con coordinate di 1e5), con i dati in `double`
    come Tecnigrafo li darebbe;
  - per ogni funzione, i casi al bordo: distanza esattamente uguale a epsilon
    (vero), il `double` successivo (falso), il pavimento contro la parte
    relativa (M sotto e sopra f / r), dati esattamente coincidenti (sempre
    vero);
  - `forUnit` con unità zero, negativa, infinita, NaN (diagnosi).
- **Valori attesi** da aritmetica esatta (`fractions` di Python): ogni `double`
  è un razionale, e il confronto tra la distanza esatta al quadrato e epsilon al
  quadrato è un confronto tra razionali.
- **Criteri di chiusura**, dal "quando è soddisfatta" della richiesta:
  - [ ] la regola è scritta (questo ADR) e le funzioni che la applicano sono
    realizzate e provate dai casi;
  - [ ] i casi danno le stesse risposte, bit per bit, sui tre sistemi operativi
    (job della CI dell'ADR 0008, punto 5);
  - [ ] il responsabile verifica dal lato di Tecnigrafo che le soglie di `core`
    si possono sostituire con la regola.
