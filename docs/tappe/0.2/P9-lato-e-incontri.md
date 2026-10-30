# [0.2-P9] `geom2d`: lato e incontri di rette e cerchi

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #…
- **Difficoltà:** impegnativa
- **Agente:** claude
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** issue `[0.2-R9]` (#…), agente codex (calcoli delicati). I suoi
  casi servono anche a Matleyx78/tecnigrafo#356. Gli incontri interni si provano
  con i test di unità qui e con i casi di R10a e R10b attraverso `intersect`
- **Dipende da:** P4, P7, R9
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#357 (con P4–P8, P10, P11)
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0010](../../adr/0010-posizione-e-intersezioni-nel-piano.md)
  (Decisione, «Che cosa rende possibili decisioni esatte»);
  `geom2d/include/solid_space/geom2d/side.hpp`, `intersection.hpp` (`Contact`);
  `robust/include/solid_space/robust/predicates.hpp`; le intestazioni interne
  `geom2d/src/curve_meaning.hpp` (P7) e `geom2d/src/exact/` (P6)

## File

- `geom2d/src/side.cpp` (nuovo)
- `geom2d/src/carriers.hpp`, `geom2d/src/carriers.cpp` (nuovi, interni)
- `tests/geom2d/side_test.cpp`, `tests/geom2d/carriers_test.cpp` (nuovi)
- `tests/corpus/esegui/op_side.cpp` (nuovo)
- `tests/corpus/in-attesa.txt` (togliere il blocco di R9)

## Cosa fare

**Pubblico.** Realizzare, senza cambiarla, `side` di `side.hpp`: il lato esatto
di un punto rispetto alla retta o al cerchio intero di una curva, nel verso della
curva, con le diagnosi dell'intestazione (`dato` 0 per la curva, 1 per il
punto). Per un segmento è `robust::orient2d`; per un cerchio e un arco il
confronto esatto tra la distanza al quadrato dal centro e il raggio al quadrato;
per un pezzo curvo lo stesso confronto con il cerchio per inizio, m e fine, in
coordinate omogenee.

**Interno** (`carriers.hpp`, namespace `solid_space::geom2d::detail`): gli
incontri esatti dei **sostegni** di due curve (rette e cerchi interi, dal
significato esatto di P7), che P10, P12, P14, P15 e P16 usano.

- Due rette: un punto (razionale, in coordinate omogenee), nessuno (parallele
  distinte) o la stessa retta.
- Retta e cerchio, due cerchi: nessun punto, un punto di tangenza, due punti
  della forma (a ± b·√d) / c con la stessa radice, o lo stesso cerchio
  (cerchi con lo stesso centro e lo stesso raggio, decisi esattamente).
- Per ogni punto, il `Contact` (incrocio o tangenza) deciso esattamente.
- Un **punto esatto** che si può confrontare con altri punti (stessa radice o
  radici diverse, con P6), arrotondare a `double` entro `kRelativePointError`, e
  dire se sta su un sostegno.
- **Coerenza con `side`:** un punto esatto sta su un sostegno esattamente quando
  `side` darebbe `on` (le due risposte vengono dallo stesso confronto).

Il filtro in virgola mobile, se serve per le prestazioni, è tuo, con il limite
d'errore scritto nel commento.

### Casi di verifica (per R9)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `side` | `curva`, `punto` | `lato`: `"left"`, `"on"` o `"right"`; oppure `diagnosi` (`dato` 0 o 1) |

R9 copre almeno:

- **#20, caso 7:** un punto a 1e-13 da un segmento, dai due lati, e lo stesso
  caso traslato di (1e6, 1e6) e scalato di 1e-3 (con i dati arrotondati come
  Tecnigrafo li darebbe: la risposta è quella esatta per quei `double`);
- per ogni tipo di curva: punti esattamente sul sostegno (`on`, scelti con
  coordinate razionali esatte: per esempio (3, 4) sul cerchio di raggio 5) e a un
  ulp da esso, dentro e fuori, sul prolungamento di un segmento e sul resto del
  cerchio di un arco;
- archi e pezzi curvi nei due versi (dentro è sinistra per il verso antiorario,
  destra per l'orario);
- coordinate minuscole e subnormali; un punto non valido (`dato` 1) e una curva
  ridotta a un punto (`dato` 0).

I valori attesi vengono da `fractions` di Python: il lato è il segno di un
polinomio nei `double` ricevuti. Per gli archi vale la regola del README («Gli
archi»): il lato rispetto al cerchio intero non dipende da D0 e D1.

## Regole della tappa che valgono qui

- **Contratto:** `side.hpp` non cambia, commenti compresi. Se qualcosa manca o
  non torna, fermati e scrivilo nella issue (etichetta `contratto`).
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, niente funzioni
  trigonometriche di `<cmath>`.
- Decisioni esatte, nessuna soglia: la regola delle tolleranze non entra qui.
- Le intestazioni interne di P6 e P7 si usano, non si cambiano; `carriers.hpp`
  la usano P10, P12, P14, P15 e P16: tienila documentata.
- File comuni: solo `tests/corpus/in-attesa.txt`, dove togli i blocchi dei tuoi
  riferimenti e nient'altro (piano, sezione 6). Nessun `CMakeLists.txt`: sorgenti
  e test li raccoglie la compilazione preparata da P3 (i test di unità in
  `tests/<componente>/`, le operazioni dei casi in `tests/corpus/esegui/`).

## Test richiesti

- `carriers_test.cpp`: per ogni coppia di sostegni, incroci, tangenze esatte e a
  un ulp, nessun incontro, stesso sostegno; rette quasi parallele con il punto
  lontanissimo; cerchi tangenti dentro e fuori; ogni punto trovato sta su
  entrambi i sostegni (verificato con l'aritmetica esatta) e scambiare i sostegni
  dà gli stessi punti.
- `side_test.cpp`: `side` dà `on` per ogni punto esatto che gli incontri trovano
  e che è un `double` (per esempio gli incroci di rette con coordinate
  razionali esatte).
- I casi di R9 passano.

## Fatto quando

- `side` è realizzata come dichiara `side.hpp`.
- Gli incontri dei sostegni sono nell'intestazione interna, documentati e
  provati dai test di unità.
- I casi di `[0.2-R9]` passano e il loro blocco è tolto da
  `in-attesa.txt`.
- Il job del determinismo è verde.
- La CI è verde.
