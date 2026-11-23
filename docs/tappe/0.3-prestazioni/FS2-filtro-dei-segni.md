# [0.3-FS2] `geom2d`: filtro in virgola mobile sui segni esatti

- **Tappa:** fuori tappa, correzione della 0.3 — [piano](../0.3-prestazioni.md)
- **Issue:** #308
- **Difficoltà:** impegnativa
- **Agente:** claude; rivede Codex, in fascia forte
- **Modello e impegno:** forte (Opus 5.5 alto); impegno alto
- **Riferimenti:** non servono: il comportamento non cambia; la verifica sono
  il corpus, i test che esistono già e i test di confronto di questa scheda
- **Contratto:** nessuna intestazione cambia
- **Dipende da:** FS1 (#307)
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0010](../../adr/0010-posizione-e-intersezioni-nel-piano.md),
  «Che cosa rende possibili decisioni esatte» e punto 8 (il filtro è previsto);
  [ADR 0008](../../adr/0008-componente-geom2d.md), punto 5 (determinismo);
  [ADR 0013](../../adr/0013-esatto-dove-serve.md); `geom2d/src/carriers.hpp` e
  `local_order.hpp` (dicono dove va il filtro); i filtri che esistono già, con
  i loro limiti scritti: `geom2d/src/intersection.cpp` e
  `geom2d/src/tangency.cpp`; `cmake/SolidSpaceDeterminism.cmake`

## File

- `geom2d/src/carriers.hpp`, `geom2d/src/carriers.cpp`
- `geom2d/src/local_order.cpp`
- `geom2d/src/positions.cpp`
- `geom2d/src/exact/quadratic.hpp`, `geom2d/src/exact/quadratic.cpp`
- se serve, un file nuovo interno per gli intervalli:
  `geom2d/src/exact/interval.hpp` e `.cpp`
- `tests/geom2d/carriers_test.cpp`, `tests/geom2d/local_order_test.cpp`,
  `tests/geom2d/positions_test.cpp`, `tests/geom2d/exact_test.cpp`; un file
  nuovo `tests/geom2d/sign_filter_test.cpp`

## Cosa fare

Oggi ogni segno si calcola con i numeri esatti (`Dyadic`), anche quando i dati
sono lontanissimi dal caso dubbio. Si aggiunge **un filtro davanti ai segni**:
prima il conto in `double` con un limite d'errore, e solo se il limite non
basta a decidere il conto esatto di oggi.

1. **Le funzioni da filtrare**, nell'ordine del peso nel profilo (#304):
   `compareX`, `compareY` e `samePoint`; `lineSign`, `circleSign`,
   `carrierSign` e `onCarrier`; `compareAlong` e `locate` di `positions.cpp`;
   i confronti di `local_order.cpp` (`compareDepartures`). Ognuna conserva
   firma e significato.
2. **L'approssimazione di un `ExactPoint`**: un intervallo di `double` per
   coordinata che contiene di sicuro il valore esatto, calcolato una volta
   (quando il punto nasce, o alla prima richiesta) e tenuto con il punto. Un
   punto che non sta nei `double` (oltre il dominio, subnormale) non ha
   intervallo e va sempre all'esatto.
3. **La regola del filtro, senza eccezioni:** il filtro risponde solo
   `positivo` o `negativo`, e solo quando l'intervallo del risultato non
   contiene lo zero; non risponde mai `zero`. In ogni altro caso chiama il
   percorso esatto, che resta nel codice invariato e raggiungibile dai test.
4. **Ogni limite d'errore è scritto e dimostrato** in un commento accanto alla
   formula, come in `tangency.cpp`: quali operazioni, quanti arrotondamenti,
   quale costante.
5. **Solo operazioni IEEE correttamente arrotondate**: somma, differenza,
   prodotto, confronto, `std::fabs`, `std::nextafter`; nessuna funzione di
   `libm`, nessuna radice salvo `std::sqrt`, nessun `long double`. La build
   vieta già `fast-math` e le contrazioni.

Perché il determinismo non è in gioco: il filtro, quando risponde, dà il segno
che darebbe il conto esatto. Se su un sistema rinunciasse dove un altro
risponde, cambierebbe solo il tempo.

## Regole che valgono qui

Sono quelle del [piano](../0.3-prestazioni.md), sezione 2:

- nessuna intestazione pubblica cambia, nemmeno un commento; nessuna
  dipendenza nuova, nessun thread;
- una scatola, un indice o un filtro salta un calcolo solo quando l'esito è
  certo, con il margine o il limite d'errore scritto e giustificato in un
  commento accanto al codice; altrimenti si esegue il percorso esatto di oggi;
- `tests/corpus/` e `tests/prestazioni/` non si toccano; i test che esistono
  non si modificano, se ne aggiungono;
- prima di cambiare codice, compila `main` e metti da parte
  `<build>/corpus/risultati.txt` e l'uscita del banco: sono il termine di
  confronto.

## Test richiesti

`sign_filter_test.cpp` confronta, per **ogni** funzione filtrata, la risposta
filtrata con quella del percorso esatto su:

- qualche centinaio di migliaia di ingressi generati da un seme fisso (lo
  schema del generatore del banco), con punti razionali e punti con radicale
  (incontri retta–cerchio e cerchio–cerchio);
- ingressi quasi degeneri: punti a distanza relativa 2^-30, 2^-40, 2^-52 e
  2^-60 da una retta, da un cerchio e fra loro;
- zeri esatti: punti che stanno sul supporto, punti uguali con
  rappresentazioni diverse;
- estremi del dominio: coordinate 2^50, 2^-50, 0, subnormali, e punti oltre il
  dominio (rette quasi parallele).

Nessuna differenza è ammessa. Un secondo test conta quante volte il filtro
decide sui dati ordinari del primo gruppo e fallisce sotto il 90%: un filtro
che non decide quasi mai non serve.

## Fatto quando

- Ogni funzione dell'elenco ha il filtro con il limite dimostrato, e il test
  di confronto la copre.
- Il banco migliora su tutte le righe che non usano l'ellisse; la pull request
  riporta in particolare `clipToRegion`, `detectBoundary`, `intersectAlong`,
  `offsetPolyline` e `snap: 32 pairs`.
- `risultati.txt` del ramo è identico byte per byte a quello di `main` al
  commit di partenza (lo dice la pull request, con il comando usato), e il
  checksum del banco è `15802239.315`.
- La pull request riporta la tabella del banco prima e dopo, stessa macchina e
  stessa build Release, e dice che cosa pesa ancora nel profilo se l'obiettivo
  non è raggiunto.
- La CI è verde sui tre sistemi, con ASan e «Determinism».
