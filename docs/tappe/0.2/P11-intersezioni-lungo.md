# [0.2-P11] `geom2d`: intersezioni lungo una curva

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #…
- **Difficoltà:** impegnativa
- **Agente:** claude
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** issue `[0.2-R11]` (#…), agente codex (calcoli delicati: due
  radici diverse). I suoi casi servono anche a Matleyx78/tecnigrafo#356
- **Dipende da:** P10, R11
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#357 (con P4–P10): di norma è
  l'ultimo pacchetto della facciata delle intersezioni
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0010](../../adr/0010-posizione-e-intersezioni-nel-piano.md)
  punti 2, 3 e 6, «Che cosa rende possibili decisioni esatte», e il rischio in
  «Conseguenze»; `geom2d/include/solid_space/geom2d/intersection.hpp`
  (`intersectAlong` e i suoi tipi); le intestazioni interne
  `geom2d/src/positions.hpp` (P10), `geom2d/src/carriers.hpp` (P9) e
  `geom2d/src/exact/quadratic.hpp` (P6); `geom2d/src/intersection.cpp` (P10)

## File

- `geom2d/src/intersect_along.cpp` (nuovo)
- `geom2d/src/positions.hpp`, `geom2d/src/positions.cpp` (di P10: solo aggiunte,
  se servono)
- `geom2d/CMakeLists.txt`
- `tests/geom2d/intersect_along_test.cpp` (nuovo)
- `tests/corpus/esegui/op_intersect_along.cpp` (nuovo)
- `tests/CMakeLists.txt`
- `tests/corpus/in-attesa.txt` (togliere le righe di R11)

## Cosa fare

Realizzare, senza cambiarla, `intersectAlong` di `intersection.hpp` con i suoi
tipi (`Cut`, `CutterContact`, `CutterOverlap`, `SkippedCutter`,
`HostIntersections`), come la vogliono `TRIM` ed `EXTEND`:

- **un solo ordine esatto lungo l'ospite** per i punti di tutti i taglienti: due
  punti di taglienti diversi hanno in generale radici diverse, e si confrontano
  con le funzioni a due radici di P6 (è la parte delicata che l'ADR 0010 indica
  tra i rischi);
- **i punti esattamente coincidenti** di taglienti diversi formano un solo `Cut`,
  con i taglienti in ordine di indice; punti distinti, anche vicinissimi, restano
  distinti;
- **ogni tagliente da solo** dà gli stessi punti, con gli stessi bit, di
  `intersect(host, hostExtension, cutter, cutterExtension)`: la via più semplice è
  riusare per ogni coppia il calcolo di P10 e poi fondere le liste in ordine
  esatto;
- **un tagliente non valido** (dati non validi, fuori dominio, ridotto a un punto)
  non fa fallire la chiamata: va in `skipped` con la sua diagnosi e `input`
  i + 1; l'ospite non valido dà la diagnosi con `input` 0;
- stazioni mai decrescenti lungo `cuts`, con le stazioni di P10, che dipendono
  solo dal punto esatto e dalla curva (piano, sezione 4): così le stazioni di un
  `Cut` sono quelle della chiamata a coppie; sovrapposizioni in ordine di inizio
  lungo l'ospite, poi di indice; `omittedBeyondDomain` sommato su tutti i
  taglienti.

### Casi di verifica (per R11)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `intersectAlong` | `curva` (l'ospite), `prolunga`, `taglienti` (elenco di curve), `prolunga_taglienti` | `tagli`: elenco di `{"punto": punto, "sull_ospite": posizione, "taglienti": [{"indice": intero, "contatto": nome, "sul_tagliente": posizione}]}`; `sovrapposizioni`: elenco di `{"tagliente": intero, "intera": booleano, "da": estremo, "a": estremo}` (estremo come in `intersect`, con `sulla_prima` l'ospite); `scartati`: elenco di `{"tagliente": intero, "diagnosi": {...}}`; `omessi_fuori_dominio`: intero; oppure `diagnosi` |

L'operazione di questo pacchetto controlla anche, per ogni tagliente, che i suoi
punti abbiano gli stessi bit della chiamata a coppie di `intersect`.

R11 copre almeno:

- **#20, caso 5:** una polilinea chiusa che si interseca in un suo vertice: per
  ogni pezzo (`pieces`) come ospite, gli altri pezzi come taglienti; i pezzi che
  arrivano al vertice danno `atStart` e `atEnd`, quello che lo attraversa un
  incrocio `inside` nello stesso punto esatto, riunito in un solo `Cut`;
- più taglienti per lo stesso punto esatto (tre rette per un punto, una retta e
  un cerchio tangenti nello stesso punto);
- punti di taglienti diversi distinti ma vicinissimi, con radici diverse (due
  cerchi che tagliano una retta in punti che differiscono di un ulp o meno), in
  un ordine che una stazione arrotondata non deciderebbe;
- taglienti scartati (non validi, fuori dominio, ridotti a un punto) in mezzo a
  taglienti buoni; un ospite non valido;
- taglienti che si sovrappongono all'ospite; rette prolungate quasi parallele
  con l'incrocio oltre il dominio;
- lo stesso caso con i taglienti in un altro ordine (stessi tagli, indici
  diversi).

I valori attesi: decisioni con `fractions` di Python e confronti tra numeri con
due radici (elevamenti al quadrato, scritti in `provenienza`); coordinate con
`decimal`.

## Regole della tappa che valgono qui

- **Contratto:** `intersection.hpp` non cambia, commenti compresi. Se qualcosa
  manca o non torna, fermati e scrivilo nella issue (etichetta `contratto`).
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, niente funzioni
  trigonometriche di `<cmath>`.
- Decisioni esatte: l'ordine e le coincidenze non si decidono mai sulle stazioni
  o sui punti arrotondati (ADR 0010, punto 4).
- Le intestazioni interne di P6, P7 e P9 si usano senza cambiarle; a quella di
  P10 (`positions.hpp`) puoi solo aggiungere. Se serve altro, fermati e scrivilo
  nella issue.
- File comuni: `geom2d/CMakeLists.txt`, `tests/CMakeLists.txt`,
  `tests/corpus/in-attesa.txt` (solo le tue righe).

## Test richiesti

- Ogni tagliente di `intersectAlong` uguale, bit per bit, alla sua chiamata a
  coppie (ADR 0010, «Verifica»), su dati generati in modo deterministico, anche
  quasi degeneri.
- Permutare i taglienti cambia solo gli indici, non i tagli né il loro ordine.
- Stazioni mai decrescenti lungo `cuts`.
- I casi di R11.

## Fatto quando

- `intersectAlong` è realizzata come dichiara `intersection.hpp`.
- I casi di `[0.2-R11]` passano e le loro righe sono tolte da `in-attesa.txt`.
- Il job del determinismo è verde.
- La CI è verde.
