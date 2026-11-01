# [0.2-P19] `geom2d`: bulge ricostruito della curva parallela

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #130
- **Difficoltà:** impegnativa
- **Agente:** claude
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** issue `[0.2-R19]` (#129), agente codex (calcoli delicati: archi
  quasi interi, estremi arrotondati)
- **Contratto:** `geom2d/include/solid_space/geom2d/offset.hpp`
- **Dipende da:** P13 (#59), P14 (#63), R19 (#129)
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#363 (con P12, P13, P14 e
  P4–P10): la P17 di Tecnigrafo non parte su un `offsetCurve` che sbaglia
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0012](../../adr/0012-curva-parallela-nel-piano.md) punti
  3, 5, 6, 7 e 9 (corretti il 2026-11-01, issue #127); `offset.hpp` (`OffsetOutcome`,
  `CurveOffset`, `offsetCurve`, `OffsetFailure`, `offsetPolyline`); i sorgenti
  di P13 e P14 elencati sotto

## File

- `geom2d/src/offset_curve.cpp`
- `geom2d/src/offset_polyline.cpp`
- `geom2d/src/offset_joints.hpp`, `geom2d/src/offset_joints.cpp`
- `tests/geom2d/offset_curve_test.cpp`, `tests/geom2d/offset_polyline_test.cpp`
- `tests/corpus/in-attesa/` (cancellare `0.2-R19.txt`)

## Cosa fare

Realizzare, senza cambiarla, la regola del bulge di `offset.hpp` (decisione del
responsabile del 2026-11-01, issue #127):

- **il bulge di ogni arco del risultato** (`offsetCurve` di un pezzo curvo;
  in `offsetPolyline` ogni pezzo di partenza, tagliato o no, e ogni arco di
  giunzione) è quello dell'arco tra gli estremi arrotondati restituiti, nel
  verso dell'arco esatto, con il centro nel punto del loro asse più vicino al
  centro esatto C del suo cerchio: con m il punto medio degli estremi, n il
  vettore dall'inizio alla fine girato di un quarto di giro in senso
  antiorario e t = ((C − m) · n) / (n · n), il `double` più vicino a
  −2t + √(4t² + 1) in senso antiorario e a −2t − √(4t² + 1) in senso orario.
  Con estremi arrotondati coincidenti, il bulge dell'arco esatto (per una
  curva sola, quello dell'ingresso);
- **non rappresentabile:** se l'arco così costruito ha un punto più lontano di
  `kRelativeOffsetError · max(M, |p|)` dall'offset esatto, o l'offset esatto un
  punto così lontano dall'arco, `offsetCurve` dà `failed` (senza curva) e
  `offsetPolyline` `failed` con `notRepresentable`, deciso mentre si compone il
  risultato, prima degli altri controlli. Mai `failed` quando entrambe le
  distanze stanno entro metà di quel limite: il controllo può essere calcolato
  in `double` con il suo margine, scritto e dimostrato in testa al file;
- `CurveOffset::outcome` e il nome `notRepresentable` nell'archivio sono già
  pronti (pull request del contratto); `tests/corpus/esegui/` non cambia.

## Regole della tappa che valgono qui

- Regola del determinismo dell'ADR 0008, punto 5: niente trigonometria di
  `<cmath>`; `sqrt` sì.
- I casi di R13 e R14 che cambiano li corregge R19, non questo pacchetto.

## Test richiesti

- Il caso della issue #127 (`offsetCurve` e `offsetPolyline`): arco quasi
  intero a x = 1e6 con corda di un ulp, ora `done` con raggio circa 0,15625 e
  ogni punto entro l'errore.
- Un caso `failed` / `notRepresentable` in cui l'arrotondamento gira la corda.
- Su dati generati: archi di ogni ampiezza, vicino all'origine e a 1e6,
  il centro dell'arco restituito vicino a quello esatto e ogni punto entro
  `kRelativeOffsetError`; semicirconferenze esatte con bulge ±1.

## Fatto quando

- Tutti i casi di R19 passano, e quelli di R13 e R14 come R19 li lascia.
- Il caso della issue #127 dà `done` e `simple` vero.
- Nessun arco del risultato fuori da `kRelativeOffsetError` nei test generati.
