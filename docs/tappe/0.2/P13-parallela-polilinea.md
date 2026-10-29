# [0.2-P13] `geom2d`: curva parallela di una polilinea

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #…
- **Difficoltà:** impegnativa
- **Agente:** claude
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** issue `[0.2-R13a]` (#…), agente codex (calcoli delicati), e
  `[0.2-R13b]` (#…), agente gemini (casi di AutoCAD). I loro casi servono anche
  a Matleyx78/tecnigrafo#362
- **Dipende da:** P10, P12, R13a, R13b
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#363 (con P12): di norma è
  l'ultimo
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0012](../../adr/0012-curva-parallela-nel-piano.md) (tutto);
  [ADR 0009](../../adr/0009-regola-delle-tolleranze-del-piano.md) punti 1 e 5;
  `geom2d/include/solid_space/geom2d/offset.hpp` (soprattutto il commento di
  `offsetPolyline`: i quattro passi, il controllo, che cosa è garantito); le
  intestazioni interne `geom2d/src/carriers.hpp` (P9), `positions.hpp` (P10),
  `curve_meaning.hpp` (P7); `geom2d/src/offset_curve.cpp` (P12)

## File

- `geom2d/src/offset_polyline.cpp` (nuovo)
- `geom2d/src/offset_joints.hpp`, `geom2d/src/offset_joints.cpp` (nuovi,
  interni, se servono a tenere il file principale leggibile)
- `geom2d/CMakeLists.txt`
- `tests/geom2d/offset_polyline_test.cpp` (nuovo)
- `tests/corpus/esegui/op_parallela_polilinea.cpp` (nuovo)
- `tests/CMakeLists.txt`
- `tests/corpus/in-attesa.txt` (togliere le righe di R13a e R13b)

## Cosa fare

Realizzare, senza cambiarla, `offsetPolyline` di `offset.hpp` con i suoi tipi
(`OffsetFailure`, `PieceOrigin`, `OffsetPiece`, `PolylineOffset`), seguendo alla
lettera il commento dell'intestazione:

1. i pezzi esattamente ridotti a un punto si tolgono prima, senza contare come
   spariti;
2. **i quattro passi sui valori esatti**: spostamento dei pezzi come
   `offsetCurve` (P12); giunzioni nel punto comune dei prolungamenti più vicino
   al vertice (confronto esatto, a parità il primo lungo A), oppure con un arco di
   raggio |d| centrato nel vertice se i pezzi di partenza sono consecutivi,
   altrimenti `failed` con `openJoint`; pezzi girati al contrario tolti, con le
   giunzioni rifatte finché nessun pezzo si toglie; pezzi del risultato ridotti a
   un punto tolti;
3. **il controllo finale sui `double` del risultato**, prima di dare `done`: due
   pezzi non consecutivi senza punti comuni (esatto, come `intersect`); due pezzi
   consecutivi che si incontrano solo nel vertice comune o in punti che
   coincidono con esso per la regola dell'ADR 0009 (`policy`); nessun incontro
   con la polilinea di partenza. Altrimenti `failed`, con il motivo;
4. `simple` vero quando nessun pezzo è sparito o è stato tolto (passi 1 e 3);
   `origins` per ogni pezzo del risultato, in ordine.

Le decisioni dei passi (dove si incontrano due curve, quale punto è più vicino
al vertice, se un pezzo gira al contrario) sono esatte, con gli incontri dei
sostegni di P9 e le posizioni di P10. Il controllo finale confronta ogni coppia di
pezzi (ADR 0012, punto 11): una struttura che riduca le coppie non serve ora.

### Casi di verifica (per R13a e R13b)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `offsetPolyline` | `polilinea`, `distanza`, `unita_in_metri` | `esito`: `"done"`, `"vanished"` o `"failed"`; `motivo`: nome di `OffsetFailure` (`"none"` se non è `failed`); `semplice`: booleano; con `done`, `polilinea`: `{"vertici": [[x, y, bulge], …], "chiusa": booleano}` con valori approssimati, e `origini`: elenco di `{"origine": "source" o "gap", "indice": intero}`; oppure `diagnosi` (`dato` 0 o 1) |

Tolleranze: i vertici `kRelativePointError · max(M, |p|)`; il bulge di un pezzo
la tolleranza che corrisponde, per la sua corda, a uno scostamento della curva di
`kRelativeOffsetError · max(M, |p|)` (chi scrive il caso la ricava e la scrive in
`provenienza`). M è la grandezza della polilinea e di |d|.

**R13a** (codex) copre almeno:

- **#22, casi 1–6:** il rettangolo 100×50 con d = 10 dentro e fuori (80×30 e
  120×70, spigoli vivi); l'asola dentro con d minore del raggio, dentro con d
  uguale o maggiore (`failed`), fuori; la L con un lato più corto di d verso
  l'interno; la polilinea con un arco interno di raggio minore di d; la U stretta
  con d maggiore di metà larghezza (`failed`, `openJoint`); e gli stessi a
  coordinate di 1e6, con le risposte della tabella dell'ADR 0012;
- i casi limite della «Verifica» dell'ADR 0012: distanza esattamente uguale al
  raggio di un pezzo (sparisce) e un ulp sotto; pezzi tangenti nei vertici,
  aperti e chiusi (nessuna giunzione: stesso estremo esatto); vertici ripetuti;
  inversione di 180° (giunzione ad arco); polilinea di un solo pezzo (come
  `offsetCurve`); distanza zero; una giunzione oltre il dominio (`beyondDomain`);
  un caso `failed` per ciascun motivo (`openJoint`, `beyondDomain`,
  `selfIntersection`, `meetsOriginal`);
- diagnosi della polilinea (`dato` 0) e della distanza (`dato` 1).

**R13b** (gemini) porta i casi a–h di `autocad-offset.dxf` (linea, cerchio,
arco, rettangolo, asola, U, conca con l'arco di raggio 3, rettangolo a 1e6):

- il file è quello del repository
  [Matleyx78/dxf-test-file](https://github.com/Matleyx78/dxf-test-file), licenza
  CC0, al commit `4ad9fff8a88e65561f3998a56a95adf921b58792`, SHA-256
  `8ffddd1f67c4ea365fb01483edcc948c08aa583136849f8dc071916d9e6d8189`; i passi e i
  punti dei comandi sono nello script `3-offset.scr` di Tecnigrafo
  (`docs/tappe/0.7/dxf-di-prova/`, licenza GPL-3.0-or-later), letto a un commit
  fissato. Entrambi entrano in `sorgenti.json` (tests/corpus/README.md, «Casi che
  arrivano da Tecnigrafo»);
- l'ingresso sono le figure di partenza, con il lato tradotto nel segno di d;
  `atteso` sono le entità che AutoCAD ha creato, lette dal DXF, con la
  tolleranza della precisione del file; provenienza `file-pubblico`;
- dove il contratto non garantisce di coincidere con AutoCAD (i casi non
  semplici, ADR 0012, punto 6), chi scrive il caso controlla con i quattro passi
  che la risposta sia la stessa; **se non lo è** (per esempio il caso g con
  distanza 5), non scrive il caso, si ferma e lo scrive nella issue: decide il
  responsabile, con l'etichetta `contratto`. Il caso f con distanza 4 («nessun
  risultato») è `failed`.

Per gli archi vale la regola del README («Gli archi»).

## Regole della tappa che valgono qui

- **Contratto:** `offset.hpp` non cambia, commenti compresi. Se qualcosa manca o
  non torna, fermati e scrivilo nella issue (etichetta `contratto`).
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, niente funzioni
  trigonometriche di `<cmath>`.
- **Mai un risultato sbagliato in silenzio** (ADR 0012, punto 5): nel dubbio,
  `failed` con il motivo.
- La regola delle tolleranze entra solo nel controllo dei pezzi consecutivi
  (ADR 0012, punto 5; alternative): non nei passi.
- Le intestazioni interne di P6, P7, P9 e P10 si usano senza cambiarle; se serve
  altro, fermati e scrivilo nella issue.
- File comuni: `geom2d/CMakeLists.txt`, `tests/CMakeLists.txt`,
  `tests/corpus/in-attesa.txt` (solo le tue righe).

## Test richiesti

Le proprietà della «Verifica» dell'ADR 0012, su dati generati in modo
deterministico:

- ogni `done` supera il controllo del passo 3 rifatto con `intersect`;
- ogni vertice di una giunzione non ad arco dista |d| dalle rette o dai cerchi
  dei due pezzi di partenza entro l'errore dichiarato;
- `offsetPolyline` di una polilinea di un pezzo concorda con `offsetCurve`;
- spostare di d e poi di −d un caso semplice senza giunzioni restituisce la
  polilinea di partenza entro il doppio dell'errore.

E i casi di R13a e R13b.

## Fatto quando

- `offsetPolyline` è realizzata come dichiara `offset.hpp`, con il controllo
  finale.
- I casi di `[0.2-R13a]` e `[0.2-R13b]` passano e le loro righe sono tolte da
  `in-attesa.txt`.
- Il job del determinismo è verde.
- La CI è verde.
