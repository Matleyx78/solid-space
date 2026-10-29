# [0.2-P14] `geom2d`: tangenti e cerchi tangenti

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #…
- **Difficoltà:** impegnativa
- **Agente:** claude-2
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** issue `[0.2-R14]` (#…), agente codex (calcoli delicati: radici
  annidate). I suoi casi servono anche a Matleyx78/tecnigrafo#366
- **Dipende da:** P10, R14
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#367 (con P15)
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0011](../../adr/0011-raccordi-e-tangenze-nel-piano.md)
  (tutto, salvo le regole del raccordo, che sono di P15);
  `geom2d/include/solid_space/geom2d/tangency.hpp` (il commento in cima,
  `kRelativeArcError`, `Placement`, `TangencyPoint`, `TangentsFromPoint`,
  `tangentsFrom`, `TangentCircle`, `TangentCircles`, `tangentCircles`),
  `intersection.hpp` (`CurvePosition`), `side.hpp`; le intestazioni interne
  `geom2d/src/carriers.hpp` (P9), `positions.hpp` (P10), `curve_meaning.hpp`
  (P7), `geom2d/src/exact/` (P6)

## File

- `geom2d/src/tangency.cpp` (nuovo)
- `geom2d/src/tangency_internal.hpp` (nuovo, interno: ciò che P15 riusa)
- `geom2d/src/exact/nested.hpp`, `geom2d/src/exact/nested.cpp` (nuovi, se servono
  i numeri con radici annidate)
- `geom2d/CMakeLists.txt`
- `tests/geom2d/tangency_test.cpp` (nuovo)
- `tests/corpus/esegui/op_tangenze.cpp` (nuovo)
- `tests/CMakeLists.txt`
- `tests/corpus/in-attesa.txt` (togliere le righe di R14)

## Cosa fare

Realizzare, senza cambiarle, `tangentsFrom` e `tangentCircles` di
`tangency.hpp`. `tangentCircleNear` e `fillet` sono di P15.

- **`tangentsFrom`:** la `Placement` esatta del punto rispetto al cerchio della
  curva; con `outside` i due punti di tangenza, con `on` il punto stesso **non
  arrotondato**, con `inside` nessuno; con `Extension::none` solo i punti sulla
  curva data; ordine esatto lungo la curva e stazioni mai decrescenti; le
  diagnosi dell'intestazione (`notCircular` per una curva dritta). È la funzione
  che lo snap chiede a ogni movimento del mouse (ADR 0011, punto 10): il caso
  comune costa qualche operazione e una radice.
- **`tangentCircles`:** i centri sono i punti comuni delle curve parallele a
  distanza r (rette spostate di ±r lungo la normale, cerchi di raggio R + r o
  |R − r|), al più otto; per ciascuno i punti di tangenza, la loro posizione su
  ciascuna curva, e il lato (`sideOfFirst`, `sideOfSecond`) come dice
  `TangentCircle`; `infinitelyMany` nei casi elencati in `TangentCircles`, con la
  lista vuota; l'ordine esatto lungo la prima curva, poi lungo la seconda.
- **Radici annidate.** La normale di un segmento porta la radice della sua
  lunghezza; il centro di un cerchio tangente a una retta spostata e a un cerchio
  spostato ha coordinate con radici annidate (ADR 0011, «Che cosa rende possibili
  decisioni esatte» e rischio). Se le funzioni di P6 non bastano, i numeri che
  servono stanno in `exact/nested.*`, con lo stesso requisito di P6: esatti per
  ogni esponente.
- **Interno** (`tangency_internal.hpp`): i candidati con le loro decisioni, nella
  forma che P15 usa per scegliere (lato, posizioni, centro esatto).

### Casi di verifica (per R14)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `tangentsFrom` | `punto`, `curva`, `prolunga` | `collocazione`: `"outside"`, `"on"` o `"inside"`; `punti`: elenco di punti di tangenza; oppure `diagnosi` (`dato` 0 o 1) |
| `tangentCircles` | `prima`, `seconda`, `raggio` | `infiniti`: booleano; `cerchi`: elenco di `{"centro": punto, "sulla_prima": punto di tangenza, "sulla_seconda": punto di tangenza, "lato_prima": nome, "lato_seconda": nome}`; oppure `diagnosi` (`dato` 0, 1 o 2) |

Un punto di tangenza (`TangencyPoint`) si scrive sempre
`{"punto": punto, "sulla_curva": posizione}`, anche in P15. Tolleranze: centri e
punti `kRelativePointError · max(M, |p|)`, stazioni
`kRelativeStationError · max(M, |s|)`; il punto di `tangentsFrom` con `on` è
esatto. R14 copre almeno:

- **#21, casi 3, 6 e 7:** due archi concentrici (nessun cerchio per ogni raggio,
  infiniti quando i raggi differiscono o si sommano di esattamente 2r); tangenti
  da un punto sul cerchio (`on`, il punto stesso) e da un punto a R + 1e-12 dal
  centro (R = 5, centro nell'origine: `outside`, due punti a y ≈ ±3.16e-6); gli
  stessi a coordinate dell'ordine di 1e6 (il punto sta `on`, come dice la tabella
  dell'ADR 0011);
- per ogni coppia di tipi (segmento, arco, cerchio, pezzo dritto e curvo):
  cerchi che entrano esattamente (un solo cerchio) e di un ulp in più o in meno;
  curve tangenti tra loro; stessa retta, stesso cerchio, rette parallele a
  distanza esattamente 2r; punti di tangenza esattamente agli estremi (`atStart`,
  `atEnd`) e oltre;
- `tangentsFrom` con il punto sul cerchio e a un ulp, dentro e fuori, con e senza
  prolungamento di un arco;
- raggio zero, negativo, NaN (diagnosi con `dato` 2); curva dritta per
  `tangentsFrom` (`notCircular`).

I valori attesi: decisioni con `fractions` di Python e confronti tra numeri
algebrici (elevamenti al quadrato, scritti in `provenienza`); centri e punti con
`decimal`. Per gli archi vale la regola del README («Gli archi»).

## Regole della tappa che valgono qui

- **Contratto:** `tangency.hpp` non cambia, commenti compresi. Se qualcosa manca
  o non torna, fermati e scrivilo nella issue (etichetta `contratto`).
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, niente funzioni
  trigonometriche di `<cmath>`.
- Il lato ha lo stesso significato di `side()` (ADR 0011, punto 3).
- Le intestazioni interne di P6, P7, P9 e P10 si usano senza cambiarle; i numeri
  con radici annidate stanno in un file nuovo, non in quelli di P6.
- File comuni: `geom2d/CMakeLists.txt`, `tests/CMakeLists.txt`,
  `tests/corpus/in-attesa.txt` (solo le tue righe).

## Test richiesti

Le proprietà della «Verifica» dell'ADR 0011, su dati generati in modo
deterministico, anche quasi degeneri:

- ogni cerchio di `tangentCircles` dista r da ciascuna curva entro l'errore
  dichiarato;
- `tangentCircles(b, a)` dà gli stessi cerchi di `tangentCircles(a, b)`, con i
  lati scambiati;
- il lato di un cerchio tangente rispetto a una curva dritta è `side()` del suo
  centro;
- `tangentsFrom` dà `on` esattamente quando `side()` dà `on` per lo stesso punto.

E i casi di R14.

## Fatto quando

- `tangentsFrom` e `tangentCircles` sono realizzate come dichiara
  `tangency.hpp`.
- I casi di `[0.2-R14]` passano e le loro righe sono tolte da `in-attesa.txt`.
- Il job del determinismo è verde.
- La CI è verde.
