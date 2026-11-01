# [0.2-P16] `geom2d`: cerchio tangente vicino ai punti e raccordo

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #70
- **Difficoltà:** impegnativa
- **Agente:** claude-2
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** issue `[0.2-R16a]` (#67, casi della richiesta) e `[0.2-R16c]`
  (#69, regole di `fillet`), agente codex (calcoli delicati); `[0.2-R16b]` (#68,
  casi di AutoCAD), agente gemini. I loro casi servono anche
  a Matleyx78/tecnigrafo#366
- **Dipende da:** P15 (#66), R16a (#67), R16b (#68), R16c (#69)
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#367 (con P4–P10, P12, P15): di
  norma è l'ultimo
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0011](../../adr/0011-raccordi-e-tangenze-nel-piano.md)
  (Decisione, punti 4–7, 9 e 11; Alternative, la scelta tra candidati);
  `geom2d/include/solid_space/geom2d/tangency.hpp` (`TangencyOutcome`,
  `ChosenCircle`, `tangentCircleNear`, `Fillet`, `fillet` e le sue quattro
  regole), `side.hpp`, `intersection.hpp`; le intestazioni interne
  `geom2d/src/tangency_internal.hpp` (P15), `positions.hpp` (P10),
  `offset_carriers.hpp` ed `exact/tower.hpp` (P12); `robust`
  `trigonometry.hpp` (`atan2`)

## File

- `geom2d/src/fillet.cpp` (nuovo)
- `geom2d/src/tangency_internal.hpp` (di P15: solo aggiunte, se servono)
- `tests/geom2d/fillet_test.cpp` (nuovo)
- `tests/corpus/esegui/op_fillet.cpp` (nuovo)
- `tests/corpus/in-attesa/` (cancellare `0.2-R16a.txt`, `0.2-R16b.txt` e `0.2-R16c.txt`)

## Cosa fare

Realizzare, senza cambiarle, `tangentCircleNear` e `fillet` di `tangency.hpp`,
con i loro tipi.

- **`tangentCircleNear`:** tra i cerchi di `tangentCircles`, quello con la somma
  più piccola di |onFirst.point − nearFirst|² + |onSecond.point − nearSecond|²,
  calcolata in `double`, in quest'ordine, sui punti arrotondati; a parità il
  primo della lista. È deterministica, non esatta (ADR 0011, punto 4 e
  alternative): l'espressione è quella scritta nell'intestazione, e nient'altro.
- **`fillet`**, le quattro regole del commento, in quest'ordine: parallele
  (il semicerchio, qualunque sia il raggio, dall'estremo della prima curva più
  vicino al punto indicato, con distanze al quadrato confrontate esattamente);
  raggio zero (lo spigolo, scelto come sceglie `tangentCircleNear`); altrimenti i
  candidati di `tangentCircles` con i lati indicati dai punti tramite `side()`
  (`ambiguousPick` se un punto sta esattamente sulla retta o sul cerchio
  dell'altra curva, anche fuori dalla curva data); l'arco più corto tra i
  punti di tangenza, con il verso deciso esattamente, nelle due forme `Arc`
  (raggio esatto, angoli da `robust::atan2`) e bulge.
- **Errori dichiarati:** centri e punti entro `kRelativePointError`, l'arco nelle
  due forme entro `kRelativeArcError`, i suoi estremi entro la stessa distanza
  dai punti di tangenza esatti.
- **Diagnosi** come dice l'intestazione, con `dato` 0 e 1 per le curve, 2 per il
  raggio, 3 e 4 per i punti indicati. Un centro di cerchio tangente (regola 3) o
  uno spigolo (regola 2) oltre `kMaxCoordinate` dà `outOfDomain` sul raggio
  (dato 2), come `tangentCircles`, e non `none` (decisione del 2026-11-01,
  [DECISIONI](../../DECISIONI.md)).

### Casi di verifica (per R16a, R16b e R16c)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `tangentCircleNear` | `prima`, `vicino_prima`, `seconda`, `vicino_seconda`, `raggio` | `esito`: nome di `TangencyOutcome`; con `found`, `cerchio`: come un elemento di `cerchi` di `tangentCircles` (P15); oppure `diagnosi` |
| `fillet` | `prima`, `punto_prima`, `seconda`, `punto_seconda`, `raggio` | `esito`; con `found`: `centro`, `raggio`, `sulla_prima` e `sulla_seconda` (punti di tangenza), `semicerchio`: booleano, `arco`: la curva attesa nella forma `{"arco": {...}}` di P13, con valori esatti o approssimati, `bulge`: approssimato; oppure `diagnosi` |

Un punto di tangenza si scrive `{"punto": punto, "sulla_curva": posizione}`, come
in P15. Tolleranze: punti `kRelativePointError · max(M, |p|)`; angoli,
ampiezza e bulge dell'arco la tolleranza che corrisponde, per il suo raggio e la
sua corda, a uno scostamento di `kRelativeArcError · max(M, |p|)` (chi scrive il
caso la ricava e la scrive in `provenienza`); il raggio di `fillet` è esatto
(quello chiesto), salvo il semicerchio.

**R16a** (codex) copre almeno:

- **#21, casi 1, 2, 4, 5 e 7:** segmenti perpendicolari, raggio 10; segmento e
  cerchio che si intersecano e che non si toccano (il raccordo esiste se e solo
  se la distanza è al più 2r, uguale a 2r un solo cerchio); raggio 1e-9 su
  segmenti quasi allineati; segmenti paralleli a distanza 20 (il semicerchio di
  raggio 10 qualunque sia il raggio chiesto); gli stessi a coordinate di 1e6,
  con le risposte della tabella dell'ADR 0011;
- `tangentCircleNear` con due candidati chiaramente diversi e con una parità
  esatta (il primo della lista).

**R16c** (codex) copre le regole di `fillet`: parallele sulla stessa retta
(`infinitelyMany`); raggio zero (lo spigolo; curve senza punti comuni: `none`;
sovrapposte: `infinitelyMany`); punto indicato esattamente sulla retta o sul
cerchio dell'altra curva, anche fuori dalla curva data (`ambiguousPick`);
raccordo che non entra (`none`); punti di tangenza diametralmente opposti
(l'arco antiorario); punti di tangenza coincidenti; le diagnosi, con il `dato`
giusto.

**R16b** (gemini) porta i casi `FILLET` a–f (angolo retto, linee che non si
toccano, parallele, cerchio e linea, arco e linea, raggio 0) e `CIRCLE` *Ttr* m e
n (due linee con raggio 8, cerchio e linea con raggio 6) di
`autocad-raccordi.dxf`:

- registra in `sorgenti.json`, con la copia obbligatoria in `esterni/`, il DXF
  del
  repository [Matleyx78/dxf-test-file](https://github.com/Matleyx78/dxf-test-file)
  (licenza CC0, commit `4ad9fff8a88e65561f3998a56a95adf921b58792`, SHA-256
  `aecc6725a0d5b0b76c11eef4c9f93613e7beda91c0a2a225496ea8899118e842`) e lo
  script `4-raccordi.scr` di Tecnigrafo (`docs/tappe/0.7/dxf-di-prova/`,
  licenza GPL-3.0-or-later, a un commit fissato di `main`), con le regole di
  `tests/corpus/README.md`, «Casi che arrivano da Tecnigrafo». Dipende da P1,
  che fa accettare le copie in `esterni/` dalla guardia della CI, e da R13, che
  scrive per primo in `sorgenti.json`;
- l'ingresso sono le figure di partenza, i punti indicati e i raggi dello
  script; `atteso` è la geometria che AutoCAD ha disegnato, letta dal DXF con la
  tolleranza della precisione del file: `esito` `found`, il centro, il raggio, i
  punti di tangenza (solo `punto`), l'arco o il cerchio. Le chiavi che il DXF non
  dice (le posizioni dei punti di tangenza, i lati) non si scrivono;
  provenienza `file-pubblico`;
- se AutoCAD disegna un arco che le regole di `fillet` non danno (per esempio
  l'arco più lungo, ADR 0011, punto 11), quel caso non si scrive: fermati su
  quel caso e scrivilo nella issue; decide il responsabile, con l'etichetta
  `contratto`.

Per gli archi vale la regola del README («Gli archi»).

## Regole della tappa che valgono qui

- **Contratto:** `tangency.hpp` non cambia, commenti compresi. Se qualcosa manca
  o non torna, fermati e scrivilo nella issue (etichetta `contratto`).
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, niente funzioni
  trigonometriche di `<cmath>`; la scelta di `tangentCircleNear` è calcolata in
  virgola mobile, quindi segue la regola anche lei.
- La scelta del lato viene da `side()` (ADR 0011, punto 4): nessuna regola
  propria.
- Le intestazioni interne di P6, P7, P9, P10 e P12 si usano senza cambiarle; a
  quella di P15 (`tangency_internal.hpp`) puoi solo aggiungere. Se serve altro,
  fermati e scrivilo nella issue.
- File comuni: nessuno; cancelli solo i file dei tuoi riferimenti in
  `tests/corpus/in-attesa/` (piano, sezione 6). Nessun `CMakeLists.txt`: sorgenti
  e test li raccoglie la compilazione preparata da P3 (i test di unità in
  `tests/<componente>/`, le operazioni dei casi in `tests/corpus/esegui/`).

## Test richiesti

- Il raccordo trovato è tra i cerchi di `tangentCircles` con i lati che dice la
  regola 3, e gli estremi dell'arco coincidono con i punti di tangenza per la
  regola dell'ADR 0009 (ADR 0011, punto 7).
- Fuori dalla regola 1, senza parità nella scelta e con punti di tangenza né
  coincidenti né diametralmente opposti, scambiare le due curve e i due punti dà
  lo stesso cerchio di raccordo, con l'arco nel verso opposto.
- L'`Arc` e il bulge descrivono lo stesso arco entro `kRelativeArcError`.
- I casi di R16a, R16b e R16c.

## Fatto quando

- `tangentCircleNear` e `fillet` sono realizzate come dichiara `tangency.hpp`.
- I casi di `[0.2-R16a]`, `[0.2-R16b]` e `[0.2-R16c]` passano e i loro file sono cancellati da `in-attesa/`.
- Il job del determinismo è verde.
- La CI è verde.
