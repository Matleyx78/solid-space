# [0.2-P10] `geom2d`: intersezioni di due curve

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #54
- **Difficoltà:** impegnativa
- **Agente:** claude
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** issue `[0.2-R10a]` (#52, casi della richiesta) e `[0.2-R10b]`
  (#53, casi limite), agente codex (calcoli delicati). I loro casi servono anche a
  Matleyx78/tecnigrafo#356
- **Dipende da:** P9 (#51), R10a (#52), R10b (#53)
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#357 (con P4–P9, P11)
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0010](../../adr/0010-posizione-e-intersezioni-nel-piano.md)
  (tutto: è il pacchetto centrale della richiesta #20);
  `geom2d/include/solid_space/geom2d/intersection.hpp`, `curves.hpp`,
  `side.hpp`; le intestazioni interne `geom2d/src/carriers.hpp` (P9),
  `geom2d/src/curve_meaning.hpp` (P7) e `geom2d/src/exact/` (P6)

## File

- `geom2d/src/intersection.cpp` (nuovo)
- `geom2d/src/positions.hpp`, `geom2d/src/positions.cpp` (nuovi, interni)
- `geom2d/src/carriers.hpp`, `geom2d/src/carriers.cpp` (di P9: solo aggiunte, se
  servono)
- `tests/geom2d/positions_test.cpp`, `tests/geom2d/intersection_test.cpp` (nuovi)
- `tests/corpus/esegui/op_intersection.cpp` (nuovo)
- `tests/corpus/in-attesa.txt` (togliere i blocchi di R10a e R10b)

## Cosa fare

Realizzare, senza cambiarla, `intersect` di `intersection.hpp` con tutti i suoi
tipi: `Extension`, `Location`, `Contact`, `CurvePosition`, `IntersectionPoint`,
`Overlap`, `PairIntersections`. `intersectAlong` è di P11.

**Interno** (`positions.hpp`, namespace `solid_space::geom2d::detail`), che P11,
P14, P15 e P16 riusano:

- la `Location` esatta di un punto esatto su una curva (prima dell'inizio,
  all'inizio, dentro, alla fine, oltre la fine), con le regole degli archi
  interi di `curves.hpp`;
- l'ordine esatto di due punti esatti lungo una curva (stessa radice o radici
  diverse, con P6), per una curva chiusa a partire dal suo inizio;
- la stazione arrotondata entro `kRelativeStationError`: una funzione del solo
  punto esatto e della curva, che non guarda la lista in cui il punto sta, e che
  non decresce lungo la curva (per esempio l'arrotondamento monotono di una
  quantità esatta che cresce con la posizione). Solo così le stazioni non
  decrescono lungo `points` **e** lo scambio degli argomenti, o un tagliente
  chiamato da solo (P11), dà gli stessi bit. Correggere una stazione guardando la
  precedente nella lista romperebbe la simmetria; se non trovi una funzione così,
  fermati e scrivilo nella issue (piano, sezione 4).

**`intersect`**, sopra gli incontri dei sostegni di P9:

- i punti dei sostegni che stanno sulle curve date (con `Extension::none`) o sui
  loro prolungamenti (con `Extension::extended`), con il `Contact` di P9 e, per
  le curve sullo stesso sostegno che si toccano in un solo punto, `sameCurve`;
- le sovrapposizioni delle curve sullo stesso sostegno: segmenti sulla stessa
  retta, archi e cerchi sullo stesso cerchio (intervalli di direzioni, decisi
  esattamente con D0 e D1), anche ridotte a un punto (che è allora un punto, non
  una sovrapposizione), e `whole` quando è l'intero sostegno;
- l'ordine lungo la prima curva; nessun punto ripetuto; gli estremi delle
  sovrapposizioni non ripetuti tra i punti;
- i punti oltre `kMaxCoordinate` contati in `omittedBeyondDomain`, non elencati;
- **simmetria bit per bit** (punto 3 dell'ADR): lo scambio degli argomenti dà gli
  stessi punti con gli stessi bit; la via più semplice è calcolare sempre in un
  ordine canonico delle due curve e scambiare i ruoli alla fine.

Il filtro in virgola mobile delle decisioni frequenti (ADR 0010, punto 8) è
tuo, con il limite d'errore nel commento; il banco di P17 lo misurerà.

### Casi di verifica (per R10a e R10b)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `intersect` | `prima`, `prolunga_prima`, `seconda`, `prolunga_seconda` | `punti`: elenco di `{"punto": punto, "contatto": nome, "sulla_prima": posizione, "sulla_seconda": posizione}`; `sovrapposizioni`: elenco di `{"intera": booleano, "da": estremo, "a": estremo}`, con estremo `{"punto": punto, "sulla_prima": posizione, "sulla_seconda": posizione}`; `omessi_fuori_dominio`: intero; oppure `diagnosi` (`dato` 0 o 1) |

Posizione e punto come in `tests/corpus/README.md`, "Dati del piano"; i punti
con tolleranza `kRelativePointError · max(M, |p|)`, le stazioni con
`kRelativeStationError · max(M, |s|)`. L'operazione di questo pacchetto chiama
anche `intersect` con gli argomenti scambiati e fa fallire il caso se i bit non
sono gli stessi (con i ruoli scambiati): ogni caso prova anche la simmetria.

**R10a** copre i casi della richiesta, **R10b** il resto; almeno:

- **#20, casi 1, 2, 3, 4, 6 e 8** (R10a): i segmenti a X; il cerchio contro y = 5,
  y = 5 + 1e-12 e y = 5 − 1e-12 (tangenza, nessun punto, due incroci a
  x ≈ ±3.16e-6); i due archi dello stesso cerchio sovrapposti per 30° (scelti
  secondo la regola «Gli archi»); il segmento a T (`atEnd` e `inside`); il
  segmento prolungato contro il cerchio (`afterEnd` a (8, 0) e (12, 0), nessun
  punto senza prolungamento); e gli stessi traslati di (1e6, 1e6) e scalati di
  1e-3, con le risposte della tabella dell'ADR 0010 (il caso 2 traslato dà tre
  tangenze);
- per ogni coppia di tipi (R10b; segmento, arco, cerchio, pezzo dritto e curvo): punti
  coincidenti, incroci agli estremi, tangenze esatte e quasi tangenze di un ulp,
  sovrapposizioni parziali, totali e ridotte a un punto, `whole`, archi quasi
  nulli e quasi interi (con la regola «Gli archi»), prolungamenti, curve
  sullo stesso sostegno che si toccano in un punto (`sameCurve`);
- rette prolungate quasi parallele con l'incrocio oltre `kMaxCoordinate`
  (`omessi_fuori_dominio` 1);
- diagnosi per ciascuna delle due curve.

Il caso 5 della #20 (polilinea che si interseca in un vertice) è di R11.

I valori attesi: decisioni con `fractions` di Python e confronti tra numeri della
forma a + b·√d; coordinate e stazioni con `decimal` ad alta precisione.

## Regole della tappa che valgono qui

- **Contratto:** `intersection.hpp` non cambia, commenti compresi. Se qualcosa
  manca o non torna, fermati e scrivilo nella issue (etichetta `contratto`).
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, niente funzioni
  trigonometriche di `<cmath>`; gli angoli delle stazioni con `robust::atan2`.
- Decisioni esatte (ADR 0010, punti 2 e 4): mai rifatte su un punto arrotondato;
  nessuna soglia.
- Le intestazioni interne di P6 e P7 si usano senza cambiarle; a quella di P9
  (`carriers.hpp`) puoi solo aggiungere, senza cambiare ciò che c'è. Se serve
  altro, fermati e scrivilo nella issue.
- `positions.hpp` la usano P11, P14, P15 e P16: tienila documentata.
- File comuni: solo `tests/corpus/in-attesa.txt`, dove togli i blocchi dei tuoi
  riferimenti e nient'altro (piano, sezione 6). Nessun `CMakeLists.txt`: sorgenti
  e test li raccoglie la compilazione preparata da P3 (i test di unità in
  `tests/<componente>/`, le operazioni dei casi in `tests/corpus/esegui/`).

## Test richiesti

Le proprietà della sezione «Verifica» dell'ADR 0010, su dati generati in modo
deterministico, anche quasi degeneri:

- simmetria bit per bit;
- `side` dà `on` per un punto di `intersect` esattamente quando il punto esatto
  sta sul sostegno (si verifica sui punti che sono `double`);
- stazioni mai decrescenti lungo `points`;
- nessuna contraddizione: per due segmenti con un incrocio `inside` su
  entrambi, `side` mette gli estremi del secondo da parti opposte della retta del
  primo; per una tangenza, `side` non li mette mai da parti opposte.

E i casi di R10a e R10b.

## Fatto quando

- `intersect` è realizzata come dichiara `intersection.hpp`.
- Le posizioni lungo le curve sono nell'intestazione interna, documentate.
- I casi di `[0.2-R10a]` e `[0.2-R10b]` passano, con la simmetria, e i loro
  blocchi sono tolti da `in-attesa.txt`.
- Il job del determinismo è verde.
- La CI è verde.
