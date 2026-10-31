# [0.2-P14] `geom2d`: curva parallela di una polilinea

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #63
- **Difficoltà:** impegnativa
- **Agente:** claude
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** issue `[0.2-R14a]` (#60, casi della richiesta e caso g) e
  `[0.2-R14c]` (#62, casi limite), agente codex (calcoli delicati); `[0.2-R14b]`
  (#61, casi di AutoCAD), agente gemini. I loro casi servono anche
  a Matleyx78/tecnigrafo#362
- **Dipende da:** P10 (#54), P12 (#57), P13 (#59), R14a (#60), R14b (#61),
  R14c (#62)
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#363 (con P4–P10, P12, P13): di
  norma è l'ultimo
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0012](../../adr/0012-curva-parallela-nel-piano.md) (tutto);
  [ADR 0009](../../adr/0009-regola-delle-tolleranze-del-piano.md) punti 1 e 5;
  `geom2d/include/solid_space/geom2d/offset.hpp` (soprattutto il commento di
  `offsetPolyline`: i quattro passi, il controllo, che cosa è garantito); le
  intestazioni interne `geom2d/src/carriers.hpp` (P9), `positions.hpp` (P10),
  `offset_carriers.hpp` e `exact/tower.hpp` (P12), `curve_meaning.hpp` (P7);
  `geom2d/src/offset_curve.cpp` (P13)

## File

- `geom2d/src/offset_polyline.cpp` (nuovo)
- `geom2d/src/offset_joints.hpp`, `geom2d/src/offset_joints.cpp` (nuovi,
  interni, se servono a tenere il file principale leggibile)
- `tests/geom2d/offset_polyline_test.cpp` (nuovo)
- `tests/corpus/esegui/op_offset_polyline.cpp` (nuovo)
- `tests/corpus/in-attesa/` (cancellare `0.2-R14a.txt`, `0.2-R14b.txt` e `0.2-R14c.txt`)

## Cosa fare

Realizzare, senza cambiarla, `offsetPolyline` di `offset.hpp` con i suoi tipi
(`OffsetFailure`, `PieceOrigin`, `OffsetPiece`, `PolylineOffset`), seguendo alla
lettera il commento dell'intestazione:

1. i pezzi esattamente ridotti a un punto si tolgono prima, senza contare come
   spariti;
2. **i quattro passi sui valori esatti**: spostamento dei pezzi come
   `offsetCurve` (P13); giunzioni nel punto comune dei prolungamenti più vicino
   al vertice (confronto esatto, a parità il primo lungo A; prolungamenti
   sovrapposti, la stessa retta o lo stesso cerchio, non si incontrano), oppure
   con un arco di raggio |d| centrato nel vertice se i pezzi di partenza sono
   consecutivi (con le tangenti nel vertice esattamente opposte, la
   semicirconferenza che continua in tangenza i pezzi spostati: oraria per d
   positiva, antioraria per d negativa), altrimenti `failed` con `openJoint`;
   pezzi girati al contrario tolti, con le giunzioni rifatte finché nessun pezzo
   si toglie (un estremo rimasto senza vicino conserva il taglio; solo un estremo
   mai tagliato resta lo spostamento del passo 1); pezzi del risultato ridotti a
   un punto tolti;
3. **il controllo finale sui `double` del risultato**, prima di dare `done`: due
   pezzi non consecutivi senza punti comuni (esatto, come `intersect`); due pezzi
   consecutivi che si incontrano solo nel vertice comune o in punti che
   coincidono con esso per la regola dell'ADR 0009 (`policy`); nessun incontro
   con la polilinea di partenza. Altrimenti `failed`, con il motivo;
4. `simple` vero quando nessun pezzo è sparito o è stato tolto (passi 1 e 3);
   `origins` per ogni pezzo del risultato, in ordine.

Le decisioni dei passi (dove si incontrano due curve, quale punto è più vicino
al vertice, se un pezzo gira al contrario) sono esatte. I pezzi spostati stanno
su sostegni spostati, i cui incontri hanno radici annidate (la normale di un
segmento obliquo porta la radice della sua lunghezza, il raggio di un pezzo
curvo è √ρ): incontri, ordine lungo un pezzo e confronto delle distanze dal
vertice vengono da `offset_carriers.hpp` di P12, le posizioni sulla polilinea di
partenza da P10. Il controllo finale confronta ogni coppia di
pezzi (ADR 0012, punto 11): una struttura che riduca le coppie non serve ora.

### Casi di verifica (per R14a, R14b e R14c)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `offsetPolyline` | `polilinea`, `distanza`, `unita_in_metri` | `esito`: `"done"`, `"vanished"` o `"failed"`; `motivo`: nome di `OffsetFailure` (`"none"` se non è `failed`); `semplice`: booleano; con `done`, `polilinea`: `{"vertici": [[x, y, bulge], …], "chiusa": booleano}` con valori approssimati, e `origini`: elenco di `{"origine": "source" o "gap", "indice": intero}`; oppure `diagnosi` (`dato` 0 o 1) |

Tolleranze: i vertici `kRelativePointError · max(M, |p|)`; il bulge di un pezzo
la tolleranza che corrisponde, per la sua corda, a uno scostamento della curva di
`kRelativeOffsetError · max(M, |p|)` (chi scrive il caso la ricava e la scrive in
`provenienza`). M è la grandezza della polilinea e di |d|.

**R14a** (codex) copre i casi della richiesta e il caso g di AutoCAD:

- **#22, casi 1–6:** il rettangolo 100×50 con d = 10 dentro e fuori (80×30 e
  120×70, spigoli vivi); l'asola dentro con d minore del raggio, dentro con d
  uguale o maggiore (`failed`), fuori; la L con un lato più corto di d verso
  l'interno (`[(0,0),(4,0),(4,20)]` con d = 5 dà `done`, `semplice` falso,
  `[(-1,5,0),(-1,20,0)]` e origine source 1: l'estremo libero conserva il
  taglio, ADR 0012 punto 4); la polilinea con un arco interno di raggio minore
  di d; la U stretta
  con d maggiore di metà larghezza (`failed`, `openJoint`); e gli stessi a
  coordinate di 1e6, con le risposte della tabella dell'ADR 0012;
- **il caso g di `autocad-offset.dxf`** (la conca con un arco di raggio 3, con
  d = 2 e d = 5): i valori attesi si calcolano con i quattro passi di
  `offset.hpp`. Con d = 5 AutoCAD disegna due archi di raggio 5, centrati negli
  estremi dell'arco sparito, che si incontrano in (473, 4); i passi, con la
  regola dell'ADR 0012 (punto 4: le curve sovrapposte non si incontrano), danno
  invece `failed` con `openJoint` e `semplice` falso, ed è questo l'atteso. La
  descrizione del caso dice che AutoCAD fa diversamente e come (decisione del
  responsabile del 2026-10-31, issue #60). Con d = 2 si confronta con ciò che
  AutoCAD ha disegnato; se differisce, R14a si ferma su quel caso e decide il
  responsabile con l'etichetta `contratto`. R14a non registra il DXF in
  `sorgenti.json`: i valori vengono dal calcolo, e la descrizione cita il file e
  il caso.

**R14c** (codex) copre i casi limite della «Verifica» dell'ADR 0012:

- distanza esattamente uguale al raggio di un pezzo (sparisce) e un ulp sotto;
  pezzi tangenti nei vertici, aperti e chiusi (nessuna giunzione: stesso estremo
  esatto); vertici ripetuti; inversione di 180° (giunzione ad arco, con il verso
  del punto 4 dell'ADR 0012: `[(0,0),(10,0),(0,0)]` con d = 1 dà `done`,
  `semplice` vero, `[(0,1,0),(10,1,-1),(10,-1,0),(0,-1,0)]` e origini source 0,
  gap 1, source 1; con d = −1 la semicirconferenza antioraria,
  `[(0,-1,0),(10,-1,1),(10,1,0),(0,1,0)]`); polilinea di
  un solo pezzo (come `offsetCurve`); distanza zero (`failed` con
  `meetsOriginal`, decisione del responsabile del 2026-10-31, issue #62);
- un caso `failed` per ciascun motivo (`openJoint`, `beyondDomain`,
  `selfIntersection`, `meetsOriginal`);
- diagnosi della polilinea (`dato` 0) e della distanza (`dato` 1).

**R14b** (gemini) porta i casi d, e, f e h di `autocad-offset.dxf` (rettangolo,
asola, U, rettangolo a 1e6); i casi a, b e c sono curve singole e stanno in R13,
il caso g in R14a:

- il DXF e lo script `3-offset.scr` li registra R13 in `sorgenti.json`, con la
  copia in `esterni/`: R14b li usa e non tocca `sorgenti.json`;
- l'ingresso sono le figure di partenza, con il lato tradotto nel segno di d
  (positivo a sinistra, nel verso della polilinea); `atteso` è la geometria che
  AutoCAD ha disegnato, letta dal DXF con la tolleranza della precisione del
  file (vertici, bulge, chiusura), e `esito`; provenienza `file-pubblico`. Le
  chiavi che il DXF non dice (`motivo`, `semplice`, `origini`) non si scrivono;
- questi casi sono semplici, salvo f con d = 4, dove AutoCAD non crea nulla e
  l'ADR 0012 (tabella, caso 5) dà `failed`. Se un caso ti sembra non semplice, o
  la geometria di AutoCAD ti sembra diversa da quella che l'ADR 0012 descrive,
  non scriverlo: fermati su quel caso e scrivilo nella issue.

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
- Le intestazioni interne di P6, P7, P9, P10 e P12 si usano senza cambiarle; se
  serve altro, fermati e scrivilo nella issue.
- File comuni: nessuno; cancelli solo i file dei tuoi riferimenti in
  `tests/corpus/in-attesa/` (piano, sezione 6). Nessun `CMakeLists.txt`: sorgenti
  e test li raccoglie la compilazione preparata da P3 (i test di unità in
  `tests/<componente>/`, le operazioni dei casi in `tests/corpus/esegui/`).

## Test richiesti

Le proprietà della «Verifica» dell'ADR 0012, su dati generati in modo
deterministico:

- ogni `done` supera il controllo del passo 3 rifatto con `intersect`;
- ogni vertice di una giunzione non ad arco dista |d| dalle rette o dai cerchi
  dei due pezzi di partenza entro l'errore dichiarato;
- `offsetPolyline` di una polilinea di un pezzo concorda con `offsetCurve`;
- spostare di d e poi di −d un caso semplice senza giunzioni restituisce la
  polilinea di partenza entro il doppio dell'errore.

E i casi di R14a, R14b e R14c.

## Fatto quando

- `offsetPolyline` è realizzata come dichiara `offset.hpp`, con il controllo
  finale.
- I casi di `[0.2-R14a]`, `[0.2-R14b]` e `[0.2-R14c]` passano e i loro file sono cancellati da `in-attesa/`.
- Il job del determinismo è verde.
- La CI è verde.
