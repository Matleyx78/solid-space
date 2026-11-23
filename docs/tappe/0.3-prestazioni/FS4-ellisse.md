# [0.3-FS4] `geom2d`: intersezioni dell'ellisse più veloci

- **Tappa:** fuori tappa, correzione della 0.3 — [piano](../0.3-prestazioni.md)
- **Issue:** #310
- **Difficoltà:** impegnativa
- **Agente:** claude; rivede Codex, in fascia forte
- **Modello e impegno:** forte (Opus 5.5 alto); impegno alto
- **Riferimenti:** non servono: il comportamento non cambia; la verifica sono
  il corpus, i test che esistono già e i test di confronto di questa scheda
- **Contratto:** nessuna intestazione cambia; usa
  `geom2d/include/solid_space/geom2d/ellipse.hpp`
- **Dipende da:** FS3 (#309)
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0017](../../adr/0017-ellisse-nel-piano.md), «Che cosa
  rende possibili decisioni esatte» e punti 7, 8 e 9;
  `geom2d/src/ellipse_intersection.cpp` (in particolare `roundedRootPoint` e
  `parameterWidth`); `geom2d/src/exact/polynomial.hpp`; il filtro di FS2

## File

- `geom2d/src/ellipse_intersection.cpp`, `geom2d/src/ellipse_conic.hpp`
- `geom2d/src/exact/polynomial.hpp`, `geom2d/src/exact/polynomial.cpp`
- `tests/geom2d/ellipse_intersection_test.cpp`,
  `tests/geom2d/polynomial_test.cpp`

## Cosa fare

Ellisse contro cerchio, arco, pezzo curvo o ellisse: il tempo va
nell'isolamento esatto delle radici della quartica (`realRoots`, circa metà),
nei segni esatti sulle radici (`signAt`, circa un terzo) e in `narrowed`.

**Il vincolo che comanda tutto il pacchetto.** Il punto di una radice si
costruisce nel punto medio dell'intervallo ristretto (`roundedRootPoint`):
dipende quindi da **quali** intervalli diadici escono da `realRoots()` e da
`narrowed()`. Perché i punti restino gli stessi bit per bit, questi intervalli
devono restare **gli stessi numeri di oggi**. Si rende più veloce ogni
decisione che porta a quegli intervalli, non la strada:

1. **Segni di un polinomio in un punto diadico** (dentro le successioni di
   Sturm, le bisezioni e `narrowed`): prima in `double` con un limite d'errore
   dimostrato (Horner con errore corrente), esatto solo se il limite non
   decide. Stessa regola di FS2: il filtro non risponde mai `zero`.
2. **`signAt` su una radice:** prima con l'intervallo della radice e
   l'aritmetica a intervalli; l'esatto solo se l'intervallo del valore contiene
   lo zero. Restringere l'intervallo per decidere è ammesso solo su una copia:
   l'intervallo che il chiamante vede non cambia.
3. **Lavoro ripetuto:** la parte senza quadrati, la successione di Sturm e la
   forma razionale dell'ellisse si calcolano una volta per coppia.
4. **Scarto prima della quartica**, dove oggi manca: curve estese e pezzi
   curvi non hanno scatola (commento in testa a `ellipse_intersection.cpp`);
   un pezzo curvo ha la scatola del suo cerchio.

Se con questo vincolo l'obiettivo non si raggiunge, **non** cambiare il modo
di costruire il punto: scrivi nella pull request quanto manca e che cosa
servirebbe. Cambiare l'ultimo bit di una coordinata è una decisione del
responsabile, che il 2026-11-23 ha scelto di non farlo.

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

- Un test che fissa, per una serie di polinomi di grado da 1 a 4 (radici
  semplici, doppie, vicine a 2^-40, coefficienti a 2^50 e 2^-50), gli estremi
  degli intervalli di `realRoots()` e di `narrowed()` come numeri diadici:
  scritto e fatto passare **prima** di cambiare `polynomial.cpp`.
- Un test di confronto fra segno filtrato ed esatto per i punti 1 e 2, su
  ingressi generati da un seme fisso, quasi degeneri e con zeri esatti (radici
  comuni).
- I test che esistono passano senza modifiche.

## Fatto quando

- Le righe `ellipse/arc`, `ellipse/circle`, `ellipse/bulge` ed
  `ellipse/ellipse ordinary` scendono di almeno tre volte sulla macchina di
  chi realizza; l'obiettivo sul computer del responsabile è «qualche decina di
  µs per coppia», e il gruppo `ellipse snap: 32 entities` sotto 1 ms con
  margine.
- Le righe `near tangent` non peggiorano.
- `risultati.txt` del ramo è identico byte per byte a quello di `main` al
  commit di partenza (lo dice la pull request, con il comando usato), e il
  checksum del banco è `15802239.315`.
- La pull request riporta la tabella del banco prima e dopo, stessa macchina e
  stessa build Release, e dice che cosa pesa ancora nel profilo se l'obiettivo
  non è raggiunto.
- La CI è verde sui tre sistemi, con ASan e «Determinism».
