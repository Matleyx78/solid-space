# [0.2-P19] `geom2d`: bulge ricostruito della curva parallela

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #130
- **Difficoltà:** impegnativa
- **Agente:** claude
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** issue `[0.2-R19]` (#129), agente codex (calcoli delicati: archi
  quasi interi, estremi arrotondati)
- **Contratto:** `geom2d/include/solid_space/geom2d/offset.hpp`
- **Dipende da:** P13 (#59), P14 (#63), C (#133), R19 (#129)
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#363 (con P12, P13, P14 e
  P4–P10): la P17 di Tecnigrafo non parte su un `offsetCurve` che sbaglia
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0012](../../adr/0012-curva-parallela-nel-piano.md) punti
  3, 5, 6, 7 e 9 (corretti il 2026-11-01, issue #127 e decisioni C #133); `offset.hpp` (`OffsetOutcome`,
  `CurveOffset`, `offsetCurve`, `OffsetFailure`, `offsetPolyline`); i sorgenti
  di P13 e P14 elencati sotto

## File

- `geom2d/src/offset_curve.cpp`
- `geom2d/src/offset_polyline.cpp`
- `geom2d/src/offset_joints.hpp`, `geom2d/src/offset_joints.cpp`
- `tests/geom2d/offset_curve_test.cpp`, `tests/geom2d/offset_polyline_test.cpp`
- `tests/corpus/in-attesa/` (cancellare `0.2-R19.txt`)

La #153 precisa che `offsetCurve` controlla estremi e raggio esatti prima
di `failed`: fuori dominio dà `outOfDomain`, dato 1, anche con estremi
arrotondati coincidenti o bulge non rappresentabile. `failed` vale solo
con quei valori esatti nel dominio; m si controlla dai double solo per
una curva restituita. `offsetPolyline` scarta invece i dritti collassati
prima dei controlli sul solo risultato finale. P19 corregge anche l'ordine
del codice P13 già unito, se necessario.

La #154 precisa che il dominio esatto si controlla dopo lo scarto dei
dritti collassati, solo sugli estremi dei pezzi rimasti. A un vertice
condiviso contano entrambi i precursori: fine esatta del precedente e inizio
esatto del successivo. Estremi appartenenti solo a pezzi scartati non contano;
m resta ricostruito dai double restituiti. P19 adegua il codice già unito
se necessario, R19 aggiunge l'esempio della #154.

## Cosa fare

Realizzare, senza cambiarla, la regola del bulge di `offset.hpp` (decisione del
responsabile del 2026-11-01, issue #127 e #132, revisione C #133):

- **tutti i vertici del risultato**, anche dei pezzi dritti e quelli calcolati
  dal codice già unito di P13 e P14, sono il double più vicino al valore esatto,
  coordinata per coordinata, con pareggio al pari: estremi spostati, tagliati,
  prolungati e di giunzione, compresi gli archi di giunzione. Non basta stare
  entro l'errore: il bulge e `notRepresentable` usano questi double;

- **il bulge di ogni arco del risultato** (`offsetCurve` di un pezzo curvo;
  in `offsetPolyline` ogni pezzo di partenza, tagliato o no, e ogni arco di
  giunzione) è quello dell'arco tra gli estremi arrotondati restituiti, nel
  verso dell'arco esatto, con il centro nel punto del loro asse più vicino al
  centro esatto C del suo cerchio: con m il punto medio degli estremi, n il
  vettore dall'inizio alla fine girato di un quarto di giro in senso
  antiorario e t = ((C − m) · n) / (n · n), il `double` più vicino a
  −2t + √(4t² + 1) in senso antiorario e a −2t − √(4t² + 1) in senso orario.
  Con estremi arrotondati coincidenti di un arco non nullo, `failed` per
  `offsetCurve`, `failed` con `notRepresentable` per `offsetPolyline`, senza
  eliminare l'arco come pezzo puntiforme;
- **non rappresentabile:** se l'arco così costruito ha un punto più lontano di
  `kRelativeOffsetError · max(M, |p|, 2^-1022)` dall'offset esatto, o l'offset esatto un
  punto così lontano dall'arco, `offsetCurve` dà `failed` (senza curva) e
  `offsetPolyline` `failed` con `notRepresentable`, deciso mentre si compone il
  risultato, prima degli altri controlli. Con estremi distinti, mai `failed`
  quando entrambe le distanze stanno entro metà di quel limite: il controllo può essere calcolato
  in `double` con il suo margine, scritto e dimostrato in testa al file;
- `CurveOffset::outcome` e il nome `notRepresentable` nell'archivio sono già
  pronti (pull request del contratto); `tests/corpus/esegui/` non cambia.


- **pezzi dritti collassati (#148):** `offsetCurve` dà `failed` senza curva
  quando gli estremi arrotondati di un segmento o pezzo dritto coincidono.
  `offsetPolyline` li scarta dopo l'arrotondamento e prima dei bulge e dei
  controlli finali, elimina le voci in `origins` e mette `simple` falso;
  se non resta nessun pezzo dà `failed` / `notRepresentable`, non `vanished`.
  La garanzia dei casi semplici non copre questo scarto;
- **dominio esatto (#149):** coordinate e raggi costruiti si controllano sul
  valore esatto, anche quando arrotonderebbero a K; un valore esatto entro K
  non si restituisce oltre K. Il punto m si ricostruisce esattamente dai
  double restituiti. Nella polilinea contano solo gli estremi dei pezzi rimasti
  dopo i passi 1–4 e lo scarto dei dritti collassati (#154), con entrambi i
  precursori esatti ai vertici condivisi; gli altri controlli finali restano
  sui double del risultato;
- **ordine dei controlli:** dati prima delle costruzioni (#150); nei controlli
  finali di `offsetPolyline` prima il dominio, poi le intersezioni nell'ordine
  dell'intestazione, e decide il primo fallimento. Correggere anche questo
  ordine se P14 già unito ne segue un altro.

## Regole della tappa che valgono qui

- Regola del determinismo dell'ADR 0008, punto 5: niente trigonometria di
  `<cmath>`; `sqrt` sì.
- I casi di R13 e R14 che cambiano li corregge R19, non questo pacchetto.

## Test richiesti

- I casi #148: segmento da (0,0) a (2^-60,2^-60), d = 1 (`failed`);
  polilinea di quel solo pezzo (`failed` / `notRepresentable`); pezzo fra due
  pezzi lunghi (scarto, origini coerenti, `simple` falso).
- I casi #149 al confine K: segmento x = K−1/8 spostato di −1/4 e cerchio
  r = K con d = −1/8 (`outOfDomain`, dato 1); stessa polilinea (`beyondDomain`).
  Verificare anche che nessun esatto entro K sia restituito oltre K.
- **#153**, K = 2^50, t = 3·2^-34: segmento (K, 0)–(K−1/8, t),
  d = −2^26: `offsetCurve` dà `outOfDomain`, dato 1, prima del collasso;
  polilinea aperta degli stessi due vertici: `failed` / `notRepresentable`.
- **#154**, stessi K, t e d: polilinea aperta (K, 0), (K−1/8, t),
  (K−1/8−2^17, t·(1+2^20)), bulge 0. Il primo pezzo si scarta:
  `done`, `simple` falso, un solo pezzo che parte da (K, 2^26).
  L'estremo esatto (K+0,09375, …) solo del pezzo scartato non conta.
- Il caso della issue #127 (`offsetCurve` e `offsetPolyline`): arco quasi
  intero a x = 1e6 con corda di un ulp, ora `done` con raggio circa 0,15625 e
  ogni punto entro l'errore.
- Il caso `failed` / `notRepresentable` della #132, ora determinato
  dall'arrotondamento al più vicino, e un arco con estremi arrotondati
  coincidenti che non deve essere scartato.
- Arrotondamento corretto di tutti i vertici: pezzi dritti e curvi, capi
  liberi, tagli, prolungamenti, giunzioni e archi di giunzione, anche nei
  percorsi di P13/P14 già uniti; casi a metà tra double per il pareggio al pari.
- Su dati generati: archi di ogni ampiezza, vicino all'origine e a 1e6,
  il centro dell'arco restituito vicino a quello esatto e ogni punto entro
  `kRelativeOffsetError`; semicirconferenze esatte con bulge ±1.

## Fatto quando

- Ogni vertice restituito è arrotondato al più vicino con pareggio al pari,
  anche nei percorsi preesistenti di P13/P14; difficoltà invariata, impegnativa.
- Scarto, fallimenti, dominio e ordine dei controlli rispettano #148–#150, #153 e #154.
- Tutti i casi di R19 passano, e quelli di R13 e R14 come R19 li lascia.
- Il caso della issue #127 dà `done` e `simple` vero.
- Nessun arco del risultato fuori da `kRelativeOffsetError` nei test generati.
