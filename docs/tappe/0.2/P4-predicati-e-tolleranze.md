# [0.2-P4] `robust`: predicati esatti e regola delle tolleranze

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #39
- **Difficoltà:** impegnativa
- **Agente:** claude
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** issue `[0.2-R4a]` (#37, `orient2d`) e `[0.2-R4b]` (#38, regola
  delle tolleranze), agente codex (calcoli delicati). I casi di R4b servono anche
  a Matleyx78/tecnigrafo#356 e #372
- **Dipende da:** P2 (#35), P3 (#36), R4a (#37), R4b (#38)
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#357 (con P5–P11) e #373 (con
  P5–P8): nella pull request, «Sblocca:» li cita se con questo pacchetto sono uniti
  tutti gli altri della loro riga (piano, sezione 6)
- **Componente:** `robust`
- **Da leggere:** [ADR 0009](../../adr/0009-regola-delle-tolleranze-del-piano.md)
  (Decisione, Contratto), [ADR 0010](../../adr/0010-posizione-e-intersezioni-nel-piano.md)
  punto 7, [ADR 0008](../../adr/0008-componente-geom2d.md) punti 5–7;
  `robust/include/solid_space/robust/predicates.hpp`, `tolerance.hpp`,
  `point.hpp`, `diagnosis.hpp`, `result.hpp`; `tests/robust/contract_test.cpp`;
  `tests/corpus/esegui/plane_data.hpp` e `operations.hpp` (P2)

## File

- `robust/src/exact_sum.hpp`, `robust/src/exact_sum.cpp` (nuovi, privati)
- `robust/src/predicates.cpp`, `robust/src/tolerance.cpp` (nuovi)
- `robust/src/robust.cpp` (si toglie: era il segnaposto)
- `tests/robust/predicates_test.cpp`, `tests/robust/tolerance_test.cpp`,
  `tests/robust/exact_sum_test.cpp` (nuovi)
- `tests/corpus/esegui/op_robust.cpp` (nuovo)
- `tests/corpus/in-attesa/` (cancellare `0.2-R4a.txt` e `0.2-R4b.txt`)

## Cosa fare

Realizzare, senza cambiarle, le dichiarazioni di `predicates.hpp`
(`orient2d`) e di `tolerance.hpp` (`TolerancePolicy`: `forUnit`, `unitInMeters`,
`floorLength`, `epsilon`, `coincident`, `negligible`, il costruttore privato),
con i comportamenti scritti nei commenti delle intestazioni e nell'ADR 0009,
punti 2–5.

**L'aritmetica esatta privata** (`exact_sum.hpp`): il segno esatto di una somma
di prodotti di due `double`, `Σ ± xᵢ·yᵢ`, **per ogni esponente**. I dati vanno da
2^-1074 a `kMaxCoordinate`: il prodotto di due valori piccoli scende sotto i
subnormali, quindi le espansioni di `double` non bastano (per esempio
`orient2d((0, 0), (1e-200, 0), (0, 1e-200))` è positivo, e un calcolo in `double`
lo dà nullo). Le scelte di rappresentazione sono tue; l'intestazione privata le
documenta. **Memoria fissa, senza allocazioni:** `orient2d`, `epsilon`,
`coincident` e `negligible` sono `noexcept`, e un'allocazione fallita
terminerebbe il programma; la grandezza massima dei numeri è nota (prodotti di
due `double` con esponenti da −1074 a 1023, quindi qualche migliaio di bit), e
il commento dice il conto. `orient2d` e `coincident` si scrivono come somme di prodotti dei
`double` ricevuti (le differenze si sviluppano, non si arrotondano).

**Un filtro in virgola mobile** decide il caso comune quando il suo limite
d'errore lo permette, anche con i subnormali; altrimenti decide il calcolo
esatto. Il limite è scritto nel commento, con il ragionamento.

**`epsilon` e `forUnit`** sono calcolati come dice `tolerance.hpp`, con
operazioni che IEEE 754 arrotonda correttamente: lo stesso `double` ovunque.
`forUnit` dà `DiagnosisCode::invalidValue`, con un messaggio che dice il motivo,
nei casi elencati nell'intestazione.

### Casi di verifica (per R4a e R4b)

Operazioni, chiavi e forme (vocabolario comune in `tests/corpus/README.md`,
"Dati del piano"):

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `orient2d` | `a`, `b`, `c`: punti | `segno`: `"negative"`, `"zero"` o `"positive"` (i nomi di `Sign`) |
| `forUnit` | `unita_in_metri` | `lunghezza_minima`: esatto (il `double` di `floorLength()`), oppure `diagnosi` (`{"codice": "invalidValue"}`, senza `dato`: la diagnosi di `robust` non lo ha) |
| `epsilon` | `unita_in_metri`, `grandezza` | `epsilon`: esatto |
| `coincident` | `unita_in_metri`, `a`, `b` | `coincidono`: booleano |
| `negligible` | `unita_in_metri`, `lunghezza`, `grandezza` | `trascurabile`: booleano |

**R4a** (`orient2d`) e **R4b** (le altre quattro operazioni) coprono almeno:

- **#23, casi 1, 4 e 5** (R4b): gli estremi (10, 0) e (10, 1e-10) in millimetri vicino
  all'origine e lo stesso caso in chilometri con coordinate di 1e5
  (`coincident`); un pezzo lungo 1e-14 alle due scale (`negligible`);
- `orient2d` (R4a): punti esattamente allineati, quasi allineati (un ulp, e il punto a
  1e-13 da un segmento del caso 7 della #20), coordinate minuscole e subnormali,
  coordinate di grandezze molto diverse, coordinate di `kMaxCoordinate`, punti
  coincidenti;
- `forUnit` (R4b) con unità 0, negativa, infinita, NaN, e con un'unità che darebbe un
  pavimento oltre `kMaxCoordinate` (diagnosi); 0.001 e 1000;
- `epsilon` con M sotto e sopra `floorLength() / kRelativeTolerance`, M = 0;
- `coincident` e `negligible` con la distanza esattamente uguale a epsilon (vero)
  e il `double` successivo (falso), e con dati esattamente coincidenti.

I valori attesi vengono da `fractions` di Python (ogni `double` è un razionale);
il procedimento è in `provenienza`.

## Regole della tappa che valgono qui

- **Contratto:** le intestazioni non cambiano, commenti compresi. Se qualcosa
  manca o non torna, fermati e scrivilo nella issue: un cambiamento è una
  decisione del responsabile (etichetta `contratto`).
- **Determinismo** (ADR 0008, punto 5): tutto il calcolo nei `.cpp`; di
  `<cmath>` solo funzioni arrotondate correttamente (`sqrt`, `fma`, `fabs`, …);
  le opzioni dei target le ha messe P3 e non si toccano.
- Decisioni esatte, nessuna soglia: le tolleranze stanno solo in `coincident`,
  `negligible` ed `epsilon`.
- Le precondizioni violate sono `assert` (ADR 0003); nessuna eccezione esce.
- Le operazioni dei casi stanno in `op_robust.cpp`; P5 ha il suo file.
- File comuni: nessuno; cancelli solo i file dei tuoi riferimenti in
  `tests/corpus/in-attesa/` (piano, sezione 6). Nessun `CMakeLists.txt`: sorgenti
  e test li raccoglie la compilazione preparata da P3 (i test di unità in
  `tests/<componente>/`, le operazioni dei casi in `tests/corpus/esegui/`).

## Test richiesti

- `exact_sum_test.cpp` e `predicates_test.cpp`: il filtro e il calcolo esatto danno lo stesso segno su
  dati generati in modo deterministico, anche quasi degeneri; permutare i tre
  punti cambia il segno come la parità della permutazione; scalare per una
  potenza di due non cambia il segno, anche verso i subnormali.
- `tolerance_test.cpp`: le precondizioni documentate, `unitInMeters` e
  `floorLength` per alcune unità, simmetria di `coincident`.
- I casi di R4a e R4b passano.

## Fatto quando

- `orient2d` e `TolerancePolicy` sono realizzate come dichiarano le intestazioni,
  senza cambiarle.
- I casi di `[0.2-R4a]` e `[0.2-R4b]` passano e i loro file sono cancellati da `in-attesa/`.
- Il job del determinismo è verde.
- La CI è verde.
