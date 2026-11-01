# [0.2-P15] `geom2d`: tangenti e cerchi tangenti

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #66
- **Difficoltà:** impegnativa
- **Agente:** claude-2
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** issue `[0.2-R15a]` (#64, `tangentsFrom`) e `[0.2-R15b]` (#65,
  `tangentCircles`), agente codex (calcoli delicati: radici annidate). I loro
  casi servono anche a Matleyx78/tecnigrafo#366
- **Dipende da:** P10 (#54), P12 (#57), R15a (#64), R15b (#65)
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#367 (con P4–P10, P12, P16)
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0011](../../adr/0011-raccordi-e-tangenze-nel-piano.md)
  (tutto, salvo le regole del raccordo, che sono di P16);
  `geom2d/include/solid_space/geom2d/tangency.hpp` (il commento in cima,
  `kRelativeArcError`, `Placement`, `TangencyPoint`, `TangentsFromPoint`,
  `tangentsFrom`, `TangentCircle`, `TangentCircles`, `tangentCircles`),
  `intersection.hpp` (`CurvePosition`), `side.hpp`; le intestazioni interne
  `geom2d/src/carriers.hpp` (P9), `positions.hpp` (P10), `offset_carriers.hpp`
  e `exact/tower.hpp` (P12), `curve_meaning.hpp` (P7), `geom2d/src/exact/` (P6)

## File

- `geom2d/src/tangency.cpp` (nuovo)
- `geom2d/src/tangency_internal.hpp` (nuovo, interno: ciò che P16 riusa)
- `tests/geom2d/tangency_test.cpp` (nuovo)
- `tests/corpus/esegui/op_tangency.cpp` (nuovo)
- `tests/corpus/in-attesa/` (cancellare `0.2-R15a.txt` e `0.2-R15b.txt`)

## Chiarimenti del contratto del 2026-11-01

La revisione C (#133; decisioni #135, #136, #140, #142) precisa: dopo le due
tangenze, l'ordine usa il centro esatto x e poi y. Un centro o punto di
tangenza da elencare costruito oltre il dominio fa fallire l'intera chiamata senza
omissioni: `outOfDomain`, dato 1 per `tangentsFrom`, dato 2 per
`tangentCircles`. Si controllano gli ingressi per numero; `notCircular`
segue la curva 0 prima del punto 1. Il pavimento generale degli errori è
2^-1022 (R20/P20); non cambiano decisioni esatte né dichiarazioni.

Le decisioni #149–#151 precisano: dominio sui valori esatti, nessun esatto
entro K restituito oltre K, senza imporre il double più vicino. Tutti i dati
si controllano prima delle costruzioni. In `tangentsFrom` si controllano solo
le tangenze elencate: con `Extension::none` quelle fuori dall'arco dato non
fanno fallire la chiamata, anche se oltre K; con `extended` si controllano
tutte. P20 adegua il codice già unito solo se necessario.

## Cosa fare

Realizzare, senza cambiarle, `tangentsFrom` e `tangentCircles` di
`tangency.hpp`. `tangentCircleNear` e `fillet` sono di P16.

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
- **Radici annidate.** I centri sono incontri di sostegni spostati, con radici
  annidate (ADR 0011, «Che cosa rende possibili decisioni esatte» e rischio):
  incontri, lati, ordine lungo una curva e numeri esatti vengono da
  `offset_carriers.hpp` ed `exact/tower.hpp` di P12.
- **Posizioni e stazioni dei punti con radici annidate.** I punti di tangenza
  di `tangentCircles` hanno radici annidate, e `positions.hpp` di P10 tratta
  solo i punti di P6 e P9. La loro `CurvePosition` la calcola questo pacchetto in
  `tangency.cpp`: la `Location` con l'ordine esatto di P12 rispetto agli estremi
  della curva, la stazione con la stessa regola di P10 (una funzione del solo
  punto esatto e della curva, che non decresce lungo la curva), a partire
  dall'arrotondamento di P12. Se la regola di P10 non si può applicare a questi
  punti senza cambiarne i file, fermati e scrivilo nella issue.
- **Interno** (`tangency_internal.hpp`): i candidati con le loro decisioni, nella
  forma che P16 usa per scegliere (lato, posizioni, centro esatto).

### Casi di verifica (per R15a e R15b)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `tangentsFrom` | `punto`, `curva`, `prolunga` | `collocazione`: `"outside"`, `"on"` o `"inside"`; `punti`: elenco di punti di tangenza; oppure `diagnosi` (`dato` 0 o 1) |
| `tangentCircles` | `prima`, `seconda`, `raggio` | `infiniti`: booleano; `cerchi`: elenco di `{"centro": punto, "sulla_prima": punto di tangenza, "sulla_seconda": punto di tangenza, "lato_prima": nome, "lato_seconda": nome}`; oppure `diagnosi` (`dato` 0, 1 o 2) |

Un punto di tangenza (`TangencyPoint`) si scrive sempre
`{"punto": punto, "sulla_curva": posizione}`, anche in P16. Tolleranze: centri e
punti `kRelativePointError · max(M, |p|, 2^-1022)`, stazioni
`kRelativeStationError · max(M, |s|, 2^-1022)`; il punto di `tangentsFrom` con `on` è
esatto. **R15a** (`tangentsFrom`) e **R15b** (`tangentCircles`) coprono almeno:

- **#21, casi 6 e 7** (R15a): tangenti da un punto sul cerchio (`on`, il punto
  stesso) e da un punto a R + 1e-12 dal centro (R = 5, centro nell'origine:
  `outside`, due punti a y ≈ ±3.16e-6); gli stessi a coordinate dell'ordine di
  1e6 (il punto sta `on`, come dice la tabella dell'ADR 0011);
- `tangentsFrom` (R15a) con il punto sul cerchio e a un ulp, dentro e fuori, con e
  senza prolungamento di un arco; curva dritta (`notCircular`); punto non valido
  (`dato` 1);
- **#21, caso 3** (R15b): due archi concentrici (nessun cerchio per ogni raggio,
  infiniti quando i raggi differiscono o si sommano di esattamente 2r), anche a
  coordinate di 1e6;
- per ogni coppia di tipi (R15b; segmento, arco, cerchio, pezzo dritto e curvo):
  cerchi che entrano esattamente (un solo cerchio) e di un ulp in più o in meno;
  curve tangenti tra loro; stessa retta, stesso cerchio, rette parallele a
  distanza esattamente 2r; punti di tangenza esattamente agli estremi (`atStart`,
  `atEnd`) e oltre;
- raggio zero, negativo, NaN (R15b: diagnosi con `dato` 2).

I valori attesi: decisioni con `fractions` di Python e confronti tra numeri
algebrici (elevamenti al quadrato, scritti in `provenienza`); centri e punti con
`decimal`. Per gli archi vale la regola del README («Gli archi»).

## Regole della tappa che valgono qui

- **Contratto:** `tangency.hpp` non cambia, commenti compresi. Se qualcosa manca
  o non torna, fermati e scrivilo nella issue (etichetta `contratto`).
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, niente funzioni
  trigonometriche di `<cmath>`.
- Il lato ha lo stesso significato di `side()` (ADR 0011, punto 3).
- Le intestazioni interne di P6, P7, P9, P10 e P12 si usano senza cambiarle; se
  serve altro, fermati e scrivilo nella issue.
- File comuni: nessuno; cancelli solo i file dei tuoi riferimenti in
  `tests/corpus/in-attesa/` (piano, sezione 6). Nessun `CMakeLists.txt`: sorgenti
  e test li raccoglie la compilazione preparata da P3 (i test di unità in
  `tests/<componente>/`, le operazioni dei casi in `tests/corpus/esegui/`).

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

E i casi di R15a e R15b.

## Fatto quando

- `tangentsFrom` e `tangentCircles` sono realizzate come dichiara
  `tangency.hpp`.
- I casi di `[0.2-R15a]` e `[0.2-R15b]` passano e i loro file sono cancellati da `in-attesa/`.
- Il job del determinismo è verde.
- La CI è verde.
