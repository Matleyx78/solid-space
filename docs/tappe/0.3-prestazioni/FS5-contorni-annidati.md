# [0.3-FS5] `geom2d`: `nestContours` più veloce

- **Tappa:** fuori tappa, correzione della 0.3 — [piano](../0.3-prestazioni.md)
- **Issue:** #311
- **Difficoltà:** media
- **Agente:** claude; rivede Codex
- **Modello e impegno:** medio (Opus 5.5 medio); impegno medio
- **Riferimenti:** non servono: il comportamento non cambia; la verifica sono
  il corpus e i test che esistono già
- **Contratto:** nessuna intestazione cambia; usa
  `geom2d/include/solid_space/geom2d/boundary.hpp` (`nestContours`)
- **Dipende da:** FS4 (#310)
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0015](../../adr/0015-parti-di-una-curva-dentro-una-regione.md)
  e il commento di `nestContours` in `boundary.hpp` (profondità, conflitti
  `same` e `crossing`, ingressi saltati); `geom2d/src/nest_contours.cpp`;
  `geom2d/src/region_parts.hpp` dopo FS1

## File

- `geom2d/src/nest_contours.cpp`
- `geom2d/src/region_parts.hpp`, `geom2d/src/region_parts.cpp` (solo se serve
  una scatola del loop intero)
- `tests/geom2d/nest_contours_test.cpp`

## Cosa fare

Oggi ogni coppia di contorni si confronta con `regionParts()` completo, nei
due versi: il tempo cresce con il quadrato dei contorni.

1. **Scatola di ogni contorno** (unione di quelle dei suoi pezzi, da FS1).
   Due contorni con scatole disgiunte non si toccano e nessuno contiene
   l'altro: profondità invariate, nessun conflitto, nessun calcolo. Le coppie
   da guardare si trovano con `overlappingPairs()`.
2. **Coppie con scatole sovrapposte, senza incontri:** se nessun pezzo
   dell'uno incontra un pezzo dell'altro (scatole dei pezzi, poi `meet()` dove
   serve) e i due non hanno tratti in comune, basta la parità di **un** punto
   di ciascun contorno rispetto all'altro. Un contorno la cui scatola non sta
   in quella dell'altro non può starci dentro, e quel verso non si calcola.
3. **Appena c'è un incontro, un contatto o un tratto comune** si usa il
   percorso di oggi, invariato: i conflitti `same` e `crossing` nascono lì.

L'ordine dei conflitti e degli ingressi saltati nel risultato resta quello di
oggi (per coppia `(i, j)` crescente): se le coppie si visitano in un altro
ordine, il risultato si riordina.

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

- Un test che confronta il risultato con quello del percorso di oggi (che
  resta raggiungibile dai test) su contorni generati da un seme fisso:
  disgiunti, annidati, tangenti dentro e fuori, uguali, che si attraversano,
  con un lato in comune, e misti di cerchi e polilinee.
- I test che esistono passano senza modifiche.

## Fatto quando

- `nestContours 100 / 200 / 300` scende di almeno tre volte sulla macchina di
  chi realizza. Il banco usa cerchi concentrici, il caso in cui ogni coppia va
  guardata: la crescita resta quadratica nel numero di coppie, cala il costo
  di ognuna.
- La pull request porta `Closes #311`; la #304 la chiude il responsabile dopo
  la misura sul suo computer.
- `risultati.txt` del ramo è identico byte per byte a quello di `main` al
  commit di partenza (lo dice la pull request, con il comando usato), e il
  checksum del banco è `15802239.315`.
- La pull request riporta la tabella del banco prima e dopo, stessa macchina e
  stessa build Release, e dice che cosa pesa ancora nel profilo se l'obiettivo
  non è raggiunto.
- La CI è verde sui tre sistemi, con ASan e «Determinism».
