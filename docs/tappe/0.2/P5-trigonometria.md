# [0.2-P5] `robust`: trigonometria deterministica

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #…
- **Difficoltà:** impegnativa
- **Agente:** claude-2
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** issue `[0.2-R5a]` (#…, `sinCos`) e `[0.2-R5b]` (#…,
  `atan2`), agente codex (calcoli delicati)
- **Dipende da:** P2, P3, R5a, R5b
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#357 (con P4, P6–P11) e #373
  (con P4, P6–P8): gli archi passano dalla trigonometria
- **Componente:** `robust`
- **Da leggere:** [ADR 0008](../../adr/0008-componente-geom2d.md) punto 5,
  [ADR 0010](../../adr/0010-posizione-e-intersezioni-nel-piano.md) (Contratto:
  `trigonometry.hpp`); `robust/include/solid_space/robust/trigonometry.hpp`;
  `tests/corpus/esegui/plane_data.hpp` e `operations.hpp` (P2)

## File

- `robust/src/trigonometry.cpp` (nuovo)
- `robust/src/trigonometry_constants.hpp` (nuovo, privato, se serve)
- `tests/robust/trigonometry_test.cpp` (nuovo)
- `tests/corpus/esegui/op_trigonometry.cpp` (nuovo)
- `tests/corpus/in-attesa.txt` (togliere i blocchi di R5a e R5b)

## Cosa fare

Realizzare, senza cambiarle, `sinCos` e `atan2` di `trigonometry.hpp`: errore al
più `kTrigMaxErrorUlps` (2 ulp) dal valore esatto, angoli di `sinCos` fino a
`kMaxTrigArgument` (2^20), zeri con segno di `atan2` come dice il commento, e gli
stessi bit su ogni piattaforma.

- **Solo operazioni arrotondate correttamente** (somme, prodotti, divisioni,
  `sqrt`, `fma`): niente `std::sin`, `std::cos`, `std::atan2`, `std::atan`,
  `std::remainder` o funzioni simili della libreria standard.
- **Riduzione dell'argomento** per |x| fino a 2^20 con π/2 diviso in più `double`
  (abbastanza parti perché il resto sia esatto o quasi in tutto il dominio); il
  commento dice quante, e perché bastano.
- **Polinomi**: i coefficienti si ricavano con un procedimento scritto nel
  commento (per esempio i coefficienti di Taylor, o un'approssimazione calcolata
  con `decimal` di Python e il suo script nella pull request), mai copiati da
  un'altra libreria (fdlibm, musl, CORE-MATH o altre): AGENTS.md, "Non copiare
  codice".
- Il commento in cima a `trigonometry.cpp` dice come si arriva ai 2 ulp:
  riduzione, polinomio, ricostruzione, con i limiti d'errore di ciascuno.

### Casi di verifica (per R5a e R5b)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `sinCos` | `angolo` | `seno`, `coseno`: approssimati |
| `atan2` | `y`, `x` | `angolo`: approssimato, oppure esatto per i valori garantiti (`atan2(+0, +0)` è 0) |

Dove conta lo zero con segno, `atteso` aggiunge per il valore la chiave
`<nome>_negativo` (`seno_negativo`, `angolo_negativo`): vero quando il risultato
ha il bit del segno (`std::signbit`). Il confronto esatto non distingue +0 da −0
(P2), e questa chiave sì. Lo zero negativo in ingresso si scrive `"-0x0p+0"`.

**Da decidere prima di R5b.** `trigonometry.hpp` dice che `atan2(0, 0)` è 0 e che
gli zeri con segno seguono lo standard C, che però dà ±π per `atan2(±0, −0)`. R5b
non scrive casi con x = −0 e y = ±0 finché il responsabile non lo decide (piano,
sezione 5); se la decisione cambia il commento dell'intestazione, è una modifica
del contratto con l'etichetta `contratto`.

La tolleranza di un valore è 2 ulp del valore esatto (2 · 2^-1074 quando il valore
esatto è 0). **R5a** copre almeno: una griglia di angoli in [−2π, 2π]; i
`double` più vicini ai multipli di π/4 e π/2 (dove seno o coseno sono quasi
zero); angoli vicini a 2^20, positivi e negativi; angoli minuscoli e subnormali;
`sinCos(−0)` (seno −0). **R5b** copre almeno: `atan2` nei quattro quadranti,
sugli assi, con rapporti y/x enormi e minuscoli, e con zeri con segno
(`atan2(+0, −1)` è il `double` più vicino a π, `atan2(−0, −1)` il suo opposto,
`atan2(−0, +1)` è −0). I valori esatti vengono da serie calcolate con `decimal`
di Python ad alta precisione; `provenienza` dice quante cifre e come.

## Regole della tappa che valgono qui

- **Contratto:** `trigonometry.hpp` non cambia, commenti compresi. Se qualcosa
  manca o non torna, fermati e scrivilo nella issue (etichetta `contratto`).
- **Determinismo** (ADR 0008, punto 5): tutto il calcolo nel `.cpp`; le opzioni
  dei target le ha messe P3 e non si toccano.
- Le precondizioni violate sono `assert`; nessuna eccezione esce.
- Non toccare `robust/src/robust.cpp`: il segnaposto lo toglie P4.
- File comuni: solo `tests/corpus/in-attesa.txt`, dove togli i blocchi dei tuoi
  riferimenti e nient'altro (piano, sezione 6). Nessun `CMakeLists.txt`: sorgenti
  e test li raccoglie la compilazione preparata da P3 (i test di unità in
  `tests/<componente>/`, le operazioni dei casi in `tests/corpus/esegui/`).

## Test richiesti

- `trigonometry_test.cpp`: `sin² + cos²` vicino a 1 su una griglia fitta;
  simmetrie (`sinCos(−x)`, `atan2(−y, x)`) esatte bit per bit dove la funzione le
  garantisce per costruzione; continuità ai bordi della riduzione; nessun
  risultato fuori da [−1, 1] o da [−π, π].
- I casi di R5a e R5b passano, e il job del determinismo li confronta sui tre sistemi.

## Fatto quando

- `sinCos` e `atan2` sono realizzate come dichiara `trigonometry.hpp`, senza
  funzioni trigonometriche della libreria standard e senza codice copiato.
- I casi di `[0.2-R5a]` e `[0.2-R5b]` passano e i loro
  blocchi sono tolti da `in-attesa.txt`.
- Il job del determinismo è verde.
- La CI è verde.
