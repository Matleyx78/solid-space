# [0.3-FS1] `geom2d`: `clipToRegion` più veloce, scatole e fasce

- **Tappa:** fuori tappa, correzione della 0.3 — [piano](../0.3-prestazioni.md)
- **Issue:** #307
- **Difficoltà:** media
- **Agente:** claude; rivede Codex
- **Modello e impegno:** medio (Opus 5.5 medio); impegno alto
- **Riferimenti:** non servono: il comportamento non cambia; la verifica sono
  il corpus e i test che esistono già
- **Contratto:** nessuna intestazione cambia; usa
  `geom2d/include/solid_space/geom2d/clipping.hpp` e `region.hpp`
- **Dipende da:** niente
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0015](../../adr/0015-parti-di-una-curva-dentro-una-regione.md),
  punti 8 e 9 (determinismo; gli indici sono ammessi «purché il risultato non
  cambi»); `geom2d/src/winding.hpp` («The method»: la semiretta verso +x);
  `geom2d/src/region_parts.cpp`; `Box`, `boxOf()` e `overlappingPairs()` in
  `geom2d/src/arrangement.hpp`, di cui riusi lo schema e i margini; il profilo
  nella #304

## File

- `geom2d/src/winding.hpp`, `geom2d/src/winding.cpp`
- `geom2d/src/region_parts.hpp`, `geom2d/src/region_parts.cpp`
- `geom2d/src/clipping.cpp`
- `tests/geom2d/winding_test.cpp`, `tests/geom2d/region_parts_test.cpp`,
  `tests/geom2d/clipping_test.cpp`

`geom2d/src/nest_contours.cpp` usa `regionParts()` e ne eredita il guadagno:
non si tocca qui (è di FS5).

## Cosa fare

Oggi `regionParts()` incontra esattamente l'ospite con **ogni** pezzo della
regione, e per **ogni** taglio `sectorStates()` ripassa tutti i pezzi due volte
(`onCarrier` e `parityAboveRay`, che ricalcola anche `partsOf` a ogni
chiamata). Il costo è righe × pezzi in aritmetica esatta.

1. **Una scatola per pezzo**, calcolata una volta quando si costruiscono i
   `LoopPieces`, con i margini di `arrangement.cpp`.
2. **Le parti monotone di ogni pezzo** (`partsOf`) calcolate una volta e
   conservate con il pezzo, non a ogni punto.
3. **Classificazione di un taglio** (`sectorStates`, `parityAboveRay`,
   `pointStatus`): un pezzo si guarda solo se la sua scatola può contenere il
   punto (appartenenza) o può essere attraversata dalla semiretta
   `y = Py, x > Px` (parità). Il punto si confronta con la scatola attraverso
   il suo arrotondamento (`nearestPoint`): il commento dimostra che il margine
   della scatola copre l'errore dell'arrotondamento, anche vicino a zero e ai
   limiti del dominio; dove non lo copre, il pezzo si guarda.
4. **Niente ripasso di tutti i pezzi per ogni taglio.** I pezzi si ordinano
   una volta per `minY` (fasce, o un ordinamento con ricerca binaria): per un
   punto si visitano solo quelli la cui fascia in `y` lo contiene.
5. **Incontri dell'ospite:** `meet()` si chiama solo per i pezzi la cui scatola
   la retta (o il cerchio) dell'ospite può toccare. Per una retta: i quattro
   vertici della scatola strettamente dallo stesso lato con
   `robust::orient2d`, che è esatto. Per righe parallele (il caso del retino)
   i pezzi si ordinano lungo la normale, così ogni riga trova i suoi senza
   scorrerli tutti (ADR 0015, punto 9).

L'ordine dei tagli, dei pezzi dentro i gruppi e delle parti nel risultato
resta quello di oggi: gli indici scelgono **quali** pezzi guardare, non
l'ordine in cui il risultato li riporta. Se una struttura cambia l'ordine di
visita, il risultato si riordina come oggi (per loop e poi per pezzo).

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

- Per `pointStatus`, `parityAboveRay` e `sectorStates`: un test che confronta
  la risposta con e senza scatole (il percorso senza scatole resta
  raggiungibile dai test) su punti generati da un seme fisso e su punti al
  limite: sul bordo di una scatola, su un vertice, all'altezza esatta di un
  estremo, a distanza 2^-40 da un pezzo, con coordinate 0, 2^50 e 2^-50.
- Per `regionParts`: righe parallele, oblique e coincidenti con un lato, su una
  regione con isole, confrontate con il percorso senza indice.
- I test che esistono passano senza modifiche.

## Fatto quando

- `clipToRegion 256 lines / 272 pieces` scende di almeno dieci volte sulla
  macchina di chi realizza (la prova usa e getta della #304 ha dato 17 volte
  con i soli punti 1, 3 e 5); l'obiettivo sul computer del responsabile è
  «pochi ms».
- `risultati.txt` del ramo è identico byte per byte a quello di `main` al
  commit di partenza (lo dice la pull request, con il comando usato), e il
  checksum del banco è `15802239.315`.
- La pull request riporta la tabella del banco prima e dopo, stessa macchina e
  stessa build Release, e dice che cosa pesa ancora nel profilo se l'obiettivo
  non è raggiunto.
- La CI è verde sui tre sistemi, con ASan e «Determinism».
