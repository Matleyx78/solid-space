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
  3, 5, 6, 7 e 9 (corretti il 2026-11-01, issue #127 e decisioni C #133);
  [ADR 0013](../../adr/0013-esatto-dove-serve.md), tutto (le costruzioni entro
  l'errore, senza il `double` più vicino, e le fasce); `offset.hpp`
  (`OffsetOutcome`, `CurveOffset`, `offsetCurve`, `OffsetFailure`,
  `offsetPolyline`); i sorgenti di P13 e P14 elencati sotto

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
m resta ricostruito dai double restituiti.

## Cosa fare

Realizzare, senza cambiarla, la regola del bulge di `offset.hpp` (decisioni del
responsabile del 2026-11-01, issue #127 e #132, revisione C #133, con l'ADR 0013
del 2026-11-02):

- **i vertici del risultato** stanno entro `kRelativePointError` dal valore
  esatto, compresi quelli dritti, tagliati, prolungati, di giunzione e degli
  archi di giunzione, anche nei percorsi già uniti di P13 e P14. Per ogni
  estremo di un pezzo curvo, anche se condiviso con un dritto, si applica
  il limite più stretto 2^-51 · max(M, |p|) per coordinata (#177). Quale
  `double` esce non è fissato (ADR 0013): il codice già unito che arrotonda al
  più vicino va bene così e non si cambia per questo. Il bulge e il controllo
  di rappresentabilità usano i double restituiti, e i vertici condivisi usano
  gli stessi double per i due pezzi;
- **il bulge di ogni arco del risultato** (`offsetCurve` di un pezzo curvo;
  in `offsetPolyline` ogni pezzo di partenza, tagliato o no, e ogni arco di
  giunzione) è quello dell'arco tra gli estremi restituiti, nel verso dell'arco
  esatto, con il centro nel punto del loro asse più vicino al centro esatto C
  del suo cerchio: con m il punto medio degli estremi, n il vettore
  dall'inizio alla fine girato di un quarto di giro in senso antiorario e
  t = ((C − m) · n) / (n · n), il bulge è −2t + √(4t² + 1) in senso antiorario
  e −2t − √(4t² + 1) in senso orario, arrotondato (quale `double` non è
  fissato). Con estremi restituiti coincidenti di un arco non nullo, `failed`
  per `offsetCurve`, `failed` con `notRepresentable` per `offsetPolyline`,
  senza eliminare l'arco come pezzo puntiforme;
- **non rappresentabile:** se l'arco restituito ha un punto più lontano di
  `kRelativeOffsetError · max(M, |p|)` dall'offset esatto, o l'offset esatto un
  punto così lontano dall'arco, `offsetCurve` dà `failed` (senza curva) e
  `offsetPolyline` `failed` con `notRepresentable`, deciso mentre si compone il
  risultato, prima degli altri controlli. Con estremi distinti, mai `failed`
  quando entrambe le distanze stanno entro metà di quel limite: il controllo
  può essere calcolato in `double` con il suo margine, scritto e dimostrato in
  testa al file;
- **pezzi dritti collassati (#148):** `offsetCurve` dà `failed` senza curva
  quando gli estremi restituiti di un segmento o pezzo dritto coincidono.
  `offsetPolyline` li scarta dopo l'arrotondamento e prima dei bulge e dei
  controlli finali, elimina le voci in `origins` e mette `simple` falso;
  se non resta nessun pezzo dà `failed` / `notRepresentable`, non `vanished`.
  Quali pezzi collassano sta nella fascia di `offset.hpp` (estremi esatti a
  meno di 2 · `kRelativePointError` · max(M, |p|) per coordinata): lì
  l'esito è libero, ma il codice deve restare coerente con i double che
  restituisce;
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
  ordine se P14 già unito ne segue un altro;
- `CurveOffset::outcome` e il nome `notRepresentable` nell'archivio sono già
  pronti (pull request del contratto); `tests/corpus/esegui/` non cambia.

## Regole della tappa che valgono qui

- Regola del determinismo dell'ADR 0008, punto 5: niente trigonometria di
  `<cmath>`; `sqrt` sì.
- I casi del corpus con valori costruiti sono già a tolleranza (pull request
  #163); quelli nuovi li scrive R19, non questo pacchetto.
- Gli esempi dentro una fascia dell'ADR 0013 non stanno nel corpus: si provano
  qui, nei test di unità, con le proprietà che valgono per ogni scelta lecita.

## Test richiesti

Esiti determinati (fuori dalle fasce):

- I casi #149 al confine K: segmento x = K−1/8 spostato di −1/4 e cerchio
  r = K con d = −1/8 (`outOfDomain`, dato 1); stessa polilinea
  (`beyondDomain`). Verificare anche che nessun esatto entro K sia restituito
  oltre K.
- **#153**, K = 2^50, t = 3·2^-34: segmento (K, 0)–(K−1/8, t),
  d = −2^26: `offsetCurve` dà `outOfDomain`, dato 1, prima del collasso.
- Su dati generati vicino all'origine e a 1e6, verificare il limite più stretto
  dei vertici e l'errore dell'arco restituito. Dimostrare la soglia #177 di
  2π − 1/4 radianti per la rappresentabilità con estremi distinti e limiti
  d'errore applicabili. Provare i casi immediatamente sotto e sopra la soglia: entro la soglia
  il bulge non dà `failed` / `notRepresentable`; oltre, verificare le
  proprietà della fascia senza fissare un esito libero. Verificare anche le
  semicirconferenze esatte con bulge ±1 entro la tolleranza del bulge.

Esempi dentro una fascia, provati con le proprietà valide per ogni scelta
lecita (l'esito è libero: si controlla che sia uno di quelli ammessi e che, con
`done`, il risultato rispetti gli errori; con `failed`, che il motivo sia quello
ammesso):

- i casi #148: segmento (0,0)–(2^-60,2^-60), d = 1; polilinea di quel solo
  pezzo; lo stesso pezzo fra due pezzi lunghi (se scartato: origini coerenti e
  `simple` falso);
- la polilinea della #153 con gli stessi due vertici, e la #154: polilinea
  aperta (K, 0), (K−1/8, t), (K−1/8−2^17, t·(1+2^20)), bulge 0, stessi K, t e
  d (se il primo pezzo si scarta: `done`, `simple` falso, un solo pezzo che
  parte da (K, 2^26), e l'estremo esatto solo del pezzo scartato non conta);
- il caso della issue #127: arco quasi intero a x = 1e6 con corda di un ulp
  (con `done`: raggio circa 0,15625 e ogni punto entro l'errore);
- il caso della #132 e un arco con estremi restituiti coincidenti, che non
  deve essere scartato come punto.

## Fatto quando

- Vertici, bulge, scarto, fallimenti, dominio e ordine dei controlli
  rispettano `offset.hpp`, con #148–#150, #153 e #154; difficoltà invariata,
  impegnativa.
- Tutti i casi di R19 passano.
- Gli esempi dentro una fascia danno un esito ammesso, con le proprietà sopra.
- Nessun arco del risultato fuori da `kRelativeOffsetError` nei test generati.
