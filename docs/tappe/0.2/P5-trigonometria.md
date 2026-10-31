# [0.2-P5] `robust`: trigonometria deterministica

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #42
- **Difficoltà:** impegnativa
- **Agente:** claude-2
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** issue `[0.2-R5a]` (#40, `sinCos`) e `[0.2-R5b]` (#41,
  `atan2`), agente codex (calcoli delicati)
- **Dipende da:** P2 (#35), P3 (#36), R5a (#40), R5b (#41)
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#357 (con P4, P6–P11) e #373
  (con P4, P6–P8): gli archi passano dalla trigonometria
- **Componente:** `robust`
- **Da leggere:** [ADR 0008](../../adr/0008-componente-geom2d.md) punto 5,
  [ADR 0010](../../adr/0010-posizione-e-intersezioni-nel-piano.md) (Contratto:
  `trigonometry.hpp`); `robust/include/solid_space/robust/trigonometry.hpp`;
  `tests/corpus/esegui/plane_data.hpp` e `operations.hpp` (P2)

## File

- `robust/include/solid_space/robust/trigonometry.hpp`: **solo** la frase sugli
  zeri del commento di `atan2`, come decide la issue `contratto` #31
- `robust/src/trigonometry.cpp` (nuovo)
- `robust/src/trigonometry_constants.hpp` (nuovo, privato, se serve)
- `tests/robust/trigonometry_test.cpp` (nuovo)
- `tests/corpus/esegui/op_trigonometry.cpp` (nuovo)
- `tests/corpus/in-attesa/` (cancellare `0.2-R5a.txt` e `0.2-R5b.txt`)

## Cosa fare

Realizzare, senza cambiarle, `sinCos` e `atan2` di `trigonometry.hpp`: errore al
più `kTrigMaxErrorUlps` (2 ulp) dal valore esatto, angoli di `sinCos` fino a
`kMaxTrigArgument` (2^20), zeri con segno di `atan2` come lo standard C (issue #31), e gli
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
| `atan2` | `y`, `x` | `angolo`: approssimato, oppure esatto per i valori garantiti (`atan2(±0, +0)` è ±0) |

Dove conta lo zero con segno, `atteso` aggiunge per il valore la chiave
`<nome>_negativo` (`seno_negativo`, `angolo_negativo`): vero quando il risultato
ha il bit del segno (`std::signbit`). Il confronto esatto non distingue +0 da −0
(P2), e questa chiave sì. Lo zero negativo in ingresso si scrive `"-0x0p+0"`.

**Gli zeri di `atan2`** (issue `contratto` #31, decisa dal responsabile il
2026-10-29). Il commento di `trigonometry.hpp` si contraddiceva («gli zeri con
segno seguono lo standard C» e «`atan2(0, 0)` è 0»). Vale lo standard C:
`atan2(±0, +0)` è ±0 e `atan2(±0, −0)` è ±π (il `double` più vicino a π, con il
segno di y). Questo pacchetto cambia la frase del commento, «atan2(0, 0) is 0»,
in «atan2(±0, +0) is ±0 and atan2(±0, −0) is ±π», e nient'altro
dell'intestazione. R5b scrive i quattro casi con questi valori; `math.atan2` di
Python dà gli stessi.

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

- **Contratto:** `trigonometry.hpp` non cambia, commenti compresi, salvo la frase
  sugli zeri di `atan2` che questa scheda autorizza (issue `contratto` #31; ADR
  0007, punto 15). Se qualcos'altro manca o non torna, fermati e scrivilo nella
  issue (etichetta `contratto`). La pull request chiude anche #31 (`Closes #31`).
- **Determinismo** (ADR 0008, punto 5): tutto il calcolo nel `.cpp`; le opzioni
  dei target le ha messe P3 e non si toccano.
- Le precondizioni violate sono `assert`; nessuna eccezione esce.
- Non toccare `robust/src/robust.cpp`: il segnaposto lo toglie P4.
- File comuni: nessuno; cancelli solo i file dei tuoi riferimenti in
  `tests/corpus/in-attesa/` (piano, sezione 6). Nessun `CMakeLists.txt`: sorgenti
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
- Il commento di `atan2` dice gli zeri come la issue #31, e il resto
  dell'intestazione non è cambiato.
- I casi di `[0.2-R5a]` e `[0.2-R5b]` passano e i loro file sono cancellati da `in-attesa/`.
- Il job del determinismo è verde.
- La CI è verde.
