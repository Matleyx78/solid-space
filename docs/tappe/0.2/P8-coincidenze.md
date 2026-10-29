# [0.2-P8] `geom2d`: coincidenze

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #…
- **Difficoltà:** media
- **Agente:** codex
- **Modello e impegno:** medio; impegno alto
- **Riferimenti:** issue `[0.2-R8a]` (#…, casi della richiesta e diagnosi) e
  `[0.2-R8b]` (#…, bordi), agente claude (calcoli delicati: i bordi
  della tolleranza). I suoi casi servono anche a Matleyx78/tecnigrafo#356 e #372
- **Dipende da:** P4, P7, R8a, R8b
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#373 (con P4–P7: di norma è
  l'ultimo) e #357 (con P4–P7, P9–P11)
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0009](../../adr/0009-regola-delle-tolleranze-del-piano.md)
  (Decisione, Contratto); `geom2d/include/solid_space/geom2d/coincidence.hpp`,
  `curves.hpp`; `robust/include/solid_space/robust/tolerance.hpp`; le
  intestazioni interne `geom2d/src/curve_meaning.hpp` (P7) e
  `geom2d/src/exact/` (P6); `robust/src/tolerance.cpp` (P4), come esempio di
  confronto esatto con epsilon

## File

- `geom2d/src/coincidence.cpp` (nuovo)
- `geom2d/CMakeLists.txt`
- `tests/geom2d/coincidence_test.cpp` (nuovo)
- `tests/corpus/esegui/op_coincidence.cpp` (nuovo)
- `tests/CMakeLists.txt`
- `tests/corpus/in-attesa.txt` (togliere le righe di R8a e R8b)

## Cosa fare

Realizzare, senza cambiarle, le cinque funzioni di `coincidence.hpp`:
`isDegenerate`, `isStraight`, `areAligned`, `areConcentric`, `onSameCircle`, con
le regole della tabella dell'ADR 0009, punto 3, e i commenti dell'intestazione.

- **M** è la grandezza di tutte le curve in gioco (`magnitude` di P7), ed
  epsilon è `policy.epsilon(M)`.
- **Ogni confronto con epsilon è esatto** (ADR 0009, punto 4): una distanza
  esatta al quadrato contro epsilon al quadrato, con i numeri di P6. Le distanze
  che coinvolgono il centro di un pezzo curvo (che è razionale) si confrontano in
  coordinate omogenee, senza dividere.
- **Le diagnosi** come dice l'intestazione: `invalidValue` e `outOfDomain` per
  ogni funzione; `degenerateCurve` dove la regola non ha senso per una curva
  ridotta a un punto (`areAligned`, `areConcentric`, `onSameCircle`);
  `notCircular` per una curva dritta dove serve un cerchio. Con due curve,
  `dato` è 0 o 1.
- **Simmetria:** `areAligned`, `areConcentric` e `onSameCircle` danno la stessa
  risposta scambiando le curve.

### Casi di verifica (per R8a e R8b)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `isDegenerate` | `curva`, `unita_in_metri` | `risposta`: booleano, oppure `diagnosi` |
| `isStraight` | `curva` (un `pezzo`), `unita_in_metri` | `risposta`, oppure `diagnosi` |
| `areAligned` | `prima`, `seconda` (due `segmento`), `unita_in_metri` | `risposta`, oppure `diagnosi` |
| `areConcentric` | `prima`, `seconda`, `unita_in_metri` | `risposta`, oppure `diagnosi` |
| `onSameCircle` | `prima`, `seconda`, `unita_in_metri` | `risposta`, oppure `diagnosi` |

**R8a** copre i casi della richiesta, i dati esattamente coincidenti e le
diagnosi; **R8b** i bordi. Almeno:

- **#23, casi 2, 3, 4 e 5** (R8a): linee lunghe 10 che divergono di 1e-12 rad
  (`areAligned`), archi con i centri a 1e-11 e lo stesso raggio
  (`areConcentric`, `onSameCircle`), un pezzo lungo 1e-14 (`isDegenerate`), in
  millimetri vicino all'origine e in chilometri con coordinate di 1e5, con le
  risposte della tabella dell'ADR 0009;
- per ogni funzione il bordo (R8b): la grandezza confrontata esattamente uguale
  a epsilon (vero) e il `double` successivo (falso), con epsilon dato dal
  pavimento e con epsilon dato dalla parte relativa. I bordi si scrivono con
  segmenti, cerchi e pezzi: con un arco la grandezza confrontata dipende da D0 e
  D1 (regola «Gli archi»); e M non deve venire dal punto m di un pezzo curvo,
  che `magnitude` arrotonda a `double` senza dire come;
- dati esattamente coincidenti, allineati, concentrici o degeneri (sempre vero);
- `isDegenerate` di un arco fino a mezzo giro (la corda), di uno più grande e di
  un cerchio (il diametro); `isStraight` con bulge 0 (sempre vero) e con la
  freccia uguale a epsilon;
- le diagnosi `degenerateCurve` e `notCircular`, con il `dato` giusto.

I valori attesi vengono da `fractions` di Python: ogni `double` è un razionale, e
ogni confronto è tra la distanza esatta al quadrato e epsilon al quadrato. Per
gli archi vale la regola del README («Gli archi»).

## Regole della tappa che valgono qui

- **Contratto:** `coincidence.hpp` non cambia, commenti compresi. Se qualcosa
  manca o non torna, fermati e scrivilo nella issue (etichetta `contratto`).
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, niente funzioni
  trigonometriche di `<cmath>`.
- La regola delle tolleranze si applica solo qui e in `robust/tolerance.hpp`:
  non entra in nessuna decisione esatta (ADR 0009, punto 1).
- Le intestazioni interne di P6 e P7 si usano, non si cambiano: se manca
  qualcosa, fermati e scrivilo nella issue.
- File comuni: `geom2d/CMakeLists.txt`, `tests/CMakeLists.txt`,
  `tests/corpus/in-attesa.txt` (solo le tue righe).

## Test richiesti

- `coincidence_test.cpp`: simmetria delle tre funzioni a due curve su dati
  generati in modo deterministico; dati esattamente coincidenti danno sempre
  vero (ADR 0009, punto 5); invertire un segmento non cambia la risposta di
  `areAligned`.
- I casi di R8a e R8b passano.

## Fatto quando

- Le cinque funzioni sono realizzate come dichiara `coincidence.hpp`, con
  confronti esatti.
- I casi di `[0.2-R8a]` e `[0.2-R8b]` passano e le loro righe sono tolte da `in-attesa.txt`.
- Il job del determinismo è verde.
- La CI è verde.
