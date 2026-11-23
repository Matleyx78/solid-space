# [0.3-FS3] `geom2d`: `detectBoundary` più veloce

- **Tappa:** fuori tappa, correzione della 0.3 — [piano](../0.3-prestazioni.md)
- **Issue:** #309
- **Difficoltà:** media
- **Agente:** claude; rivede Codex
- **Modello e impegno:** medio (Opus 5.5 medio); impegno alto
- **Riferimenti:** non servono: il comportamento non cambia; la verifica sono
  il corpus e i test che esistono già
- **Contratto:** nessuna intestazione cambia; usa
  `geom2d/include/solid_space/geom2d/boundary.hpp` e `region.hpp`
- **Dipende da:** FS2 (#308)
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0014](../../adr/0014-contorno-e-isole-di-una-regione.md),
  punto 9 (ingombri, griglia e gruppo di curve intorno al punto sono ammessi,
  «purché» il risultato non cambi) e i punti sul determinismo;
  `geom2d/src/boundary.cpp`, `boundary_rule.hpp`, `arrangement.hpp` e
  `arrangement.cpp`; il profilo nella #304

## File

- `geom2d/src/boundary.cpp`, `geom2d/src/boundary_rule.hpp`
- `geom2d/src/arrangement.hpp`, `geom2d/src/arrangement.cpp`
- `tests/geom2d/boundary_test.cpp`, `tests/geom2d/arrangement_test.cpp`

## Cosa fare

Nel profilo gli incontri fra le curve pesano il 2%. Il tempo va altrove:

1. **`convertedArc`** (circa un terzo): ogni arco candidato diventa pezzi di
   polilinea con punti arrotondati esattamente, circa 50 µs per arco. Si
   riduce il lavoro (niente conti ripetuti, niente conversioni esatte che un
   filtro di FS2 può evitare), ma **i `double` che escono sono gli stessi di
   oggi, bit per bit**: un test li fissa prima di cambiare il codice.
2. **`Arrangement::split`** (più di metà): l'ordinamento dei punti per
   `(x, y)` e i confronti lungo le curve (`compareFrom`, `compareAlong`). Dopo
   FS2 i confronti sono filtrati; qui si evita il resto: il punto arrotondato
   si calcola una volta per punto e si riusa (scatole, chiavi di ordinamento),
   `carrierOf` e `boxOf` non si ricalcolano per la stessa curva, e
   l'ordinamento confronta prima i `double` e passa all'esatto solo quando gli
   intervalli si toccano.
3. **Solo se il profilo lo chiede ancora:** limitare l'arrangiamento alle
   curve che possono contare per il punto scelto (ADR 0014, punto 9). È il
   passo più rischioso: le diagnosi, le curve saltate, gli estremi liberi e
   l'esito `notEnclosed` devono restare identici. Se non riesci a dimostrarlo,
   non farlo e scrivilo nella pull request.

L'ordine dei vertici, dei pezzi e dei loop nel risultato resta quello di oggi.

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

- Un test che fissa, per una serie di archi (ampiezze piccole, quasi un giro,
  raggi 2^-20 e 2^40, versi orario e antiorario), i pezzi di `convertedArc`
  bit per bit: scritto e fatto passare **prima** di cambiare la funzione.
- Un test dell'arrangiamento che confronta vertici, pezzi e cicli con quelli
  del percorso di oggi su disegni generati da un seme fisso, con curve che si
  toccano, coincidenti e quasi coincidenti (2^-40).
- I test che esistono passano senza modifiche.

## Fatto quando

- `detectBoundary 2400 enclosed` e `notEnclosed` scendono di almeno due volte
  rispetto a `main` dopo FS2, sulla macchina di chi realizza; l'obiettivo sul
  computer del responsabile è «poche decine di ms».
- `risultati.txt` del ramo è identico byte per byte a quello di `main` al
  commit di partenza (lo dice la pull request, con il comando usato), e il
  checksum del banco è `15802239.315`.
- La pull request riporta la tabella del banco prima e dopo, stessa macchina e
  stessa build Release, e dice che cosa pesa ancora nel profilo se l'obiettivo
  non è raggiunto.
- La CI è verde sui tre sistemi, con ASan e «Determinism».
