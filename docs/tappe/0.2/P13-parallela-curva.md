# [0.2-P13] `geom2d`: curva parallela di una curva

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #59
- **Difficoltà:** media
- **Agente:** codex
- **Modello e impegno:** medio; impegno alto
- **Riferimenti:** issue `[0.2-R13]` (#58), agente gemini (con i casi a, b e c di
  `autocad-offset.dxf`). I suoi casi servono anche a Matleyx78/tecnigrafo#362
- **Dipende da:** P7 (#46), R13 (#58)
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#363 (con P4–P10, P12, P14)
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0012](../../adr/0012-curva-parallela-nel-piano.md) punti
  2, 3, 7 e 9; `geom2d/include/solid_space/geom2d/offset.hpp` (il commento in
  cima, `kRelativeOffsetError`, `OffsetOutcome`, `CurveOffset`, `offsetCurve`),
  `curves.hpp`; le intestazioni interne `geom2d/src/curve_meaning.hpp` (P7) e
  `geom2d/src/exact/` (P6)

## File

- `geom2d/src/offset_curve.cpp` (nuovo)
- `tests/geom2d/offset_curve_test.cpp` (nuovo)
- `tests/corpus/esegui/op_offset_curve.cpp` (nuovo)
- `tests/corpus/in-attesa/` (cancellare `0.2-R13.txt`)

## Chiarimenti del contratto del 2026-11-01

La revisione C (#133; decisioni #132, #139, #141, #143, #144) precisa:
curva controllata prima della distanza; zero restituisce l'ingresso valido
con gli stessi bit; tutti gli estremi al double più vicino, pareggio al pari;
per un pezzo curvo, estremi arrotondati coincidenti danno `failed`. P19 realizza
anche l'arrotondamento di tutti gli estremi del codice P13 già unito. Il
pavimento generale degli errori è 2^-1022, con verifica R20/P20.

Le decisioni #148 e #149 aggiungono: anche un segmento o pezzo dritto
con estremi arrotondati coincidenti dà `failed`, senza curva. Il dominio di
coordinate e raggio si decide sul valore esatto; il punto m si ricostruisce
esattamente dai double restituiti. Un esatto entro K non si restituisce
oltre K. P19 adegua i percorsi già uniti, senza cambiare questo contratto.

La #153 precisa che `offsetCurve` controlla estremi e raggio esatti prima
di `failed`: fuori dominio dà `outOfDomain`, dato 1, anche con estremi
arrotondati coincidenti o bulge non rappresentabile. `failed` vale solo
con quei valori esatti nel dominio; m si controlla dai double solo per
una curva restituita. `offsetPolyline` scarta invece i dritti collassati
prima dei controlli sul solo risultato finale. P19 corregge anche l'ordine
del codice P13 già unito, se necessario.

## Cosa fare

Realizzare, senza cambiarla, `offsetCurve` di `offset.hpp`, come dice il commento
di `CurveOffset`:

- **la distanza ha il segno del lato**: positiva a sinistra, nel verso della
  curva; per un cerchio e un arco antiorario la sinistra è l'interno; zero dà la
  curva stessa, con gli stessi bit;
- un **segmento** (e un pezzo dritto) si sposta lungo la normale, con ogni
  estremo entro `kRelativePointError`;
- un **cerchio** e un **arco** tengono il centro (e l'arco angolo iniziale e
  ampiezza, quindi le stesse direzioni D0 e D1) e cambiano il raggio:
  R − d per un cerchio e un arco antiorario, R + d per un arco orario;
- un **pezzo curvo**: inizio e fine si spostano lungo il raggio del suo
  cerchio, entro `kRelativePointError`; fino al 2026-11-01 teneva il bulge, che
  ora si ricostruisce dagli estremi arrotondati con il centro più vicino a
  quello esatto, e un arco che nessun bulge rappresenta dà `failed` (decisione
  del responsabile, issue #127; lo realizza P19);
- il nuovo raggio si confronta con zero **esattamente** (con i numeri di P6, non
  sul `double` arrotondato): zero o meno dà `vanished`; per un pezzo curvo il
  raggio è quello del cerchio per inizio, m e fine;
- le diagnosi dell'intestazione: curva (`dato` 0), distanza non finita o oltre
  `kMaxCoordinate`, dato costruito fuori dal dominio di `curves.hpp` (`dato` 1): coordinate,
  raggio o punto m oltre il dominio, mai la sola estensione geometrica.

### Casi di verifica (per R13)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `offsetCurve` | `curva`, `distanza` | `esito`: `"done"`, `"vanished"` o `"failed"`; con `done`, `curva`: la curva attesa con la stessa forma dell'ingresso (`segmento`, `arco`, `cerchio`, `pezzo`), con valori esatti dove il contratto li garantisce (centro, angoli, ampiezza; bulge secondo la regola aggiornata di P19) e approssimati altrove; oppure `diagnosi` |

Le tolleranze dei punti sono `kRelativePointError · max(M, |p|, 2^-1022)`, con M la
grandezza della curva e di |d|; quella di un raggio è la stessa. R13 copre almeno:

- ogni tipo di curva, dai due lati; archi e pezzi curvi nei due versi;
- distanza zero (la curva stessa, esatta);
- distanza esattamente uguale al raggio verso l'interno (`vanished`) e un ulp
  sotto (`done`, raggio minuscolo);
- un pezzo curvo con bulge 1 e uno con bulge piccolo;
- segmenti obliqui, con la normale irrazionale;
- coordinate di 1e6;
- distanza NaN, infinita, oltre `kMaxCoordinate`; curva ridotta a un punto;
- **i casi a, b e c di `autocad-offset.dxf`** (linea con d = 5, cerchio con 3
  dentro e fuori, arco con 4 fuori e dentro): l'ingresso dallo script, `atteso`
  la geometria che AutoCAD ha disegnato, letta dal DXF con la tolleranza della
  precisione del file, provenienza `file-pubblico`.

R13 registra in `sorgenti.json` il DXF e lo script che lo ha prodotto (li usano
anche R14a e R14b, che non li registrano di nuovo) e ne mette la copia,
obbligatoria, in `esterni/` (ADR 0005, punto 3). Casi, voci e copie stanno nella
stessa pull request di riferimenti: la guardia della CI la accetta perché P1, da
cui R13 dipende, conta `esterni/` tra i riferimenti.

- `autocad-offset.dxf` del repository
  [Matleyx78/dxf-test-file](https://github.com/Matleyx78/dxf-test-file), licenza
  CC0, al commit `4ad9fff8a88e65561f3998a56a95adf921b58792`, SHA-256
  `8ffddd1f67c4ea365fb01483edcc948c08aa583136849f8dc071916d9e6d8189`;
- lo script `3-offset.scr` di Tecnigrafo (`docs/tappe/0.7/dxf-di-prova/`,
  licenza GPL-3.0-or-later), a un commit fissato di `main`, con il suo hash.

Le regole sono quelle di `tests/corpus/README.md`, «Casi che arrivano da
Tecnigrafo».

I valori attesi: confronti tra raggio e distanza con `fractions` di Python; punti
spostati con `decimal` (una radice). Per gli archi vale la regola del README
(«Gli archi»).

## Regole della tappa che valgono qui

- **Contratto:** `offset.hpp` non cambia, commenti compresi. Se qualcosa manca o
  non torna, fermati e scrivilo nella issue (etichetta `contratto`).
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, niente funzioni
  trigonometriche di `<cmath>`.
- `offsetPolyline` è di P14: non scriverla qui.
- Le intestazioni interne di P6 e P7 si usano, non si cambiano: se manca
  qualcosa, fermati e scrivilo nella issue.
- File comuni: nessuno; cancelli solo i file dei tuoi riferimenti in
  `tests/corpus/in-attesa/` (piano, sezione 6). Nessun `CMakeLists.txt`: sorgenti
  e test li raccoglie la compilazione preparata da P3 (i test di unità in
  `tests/<componente>/`, le operazioni dei casi in `tests/corpus/esegui/`).

## Test richiesti

- Spostare di d e poi di −d restituisce la curva entro il doppio dell'errore
  (esattamente per cerchi e archi, quando il raggio torna lo stesso `double`).
- Ogni estremo spostato dista |d| dalla retta o dal cerchio di partenza entro
  l'errore dichiarato.
- I casi di R13.

## Fatto quando

- `offsetCurve` è realizzata come dichiara `offset.hpp`.
- I casi di `[0.2-R13]` passano e il loro file è cancellato da `in-attesa/`.
- Il job del determinismo è verde.
- La CI è verde.
