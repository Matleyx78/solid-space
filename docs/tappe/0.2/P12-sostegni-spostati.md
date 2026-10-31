# [0.2-P12] `geom2d`: incontri esatti di rette e cerchi spostati

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #57
- **Difficoltà:** impegnativa
- **Agente:** claude-2
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** non servono: il pacchetto non ha funzioni pubbliche; si prova
  con i suoi test di unità, che verificano ogni risultato con l'aritmetica
  esatta, e, attraverso le funzioni pubbliche, con i casi di R14a, R14c, R15b,
  R16a e R16c
- **Dipende da:** P9 (#51)
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#363 (con P4–P10, P13, P14) e
  #367 (con P4–P10, P15, P16)
- **Componente:** `geom2d` (interno)
- **Da leggere:** [ADR 0011](../../adr/0011-raccordi-e-tangenze-nel-piano.md)
  («Che cosa rende possibili decisioni esatte» e il rischio in «Conseguenze»),
  [ADR 0012](../../adr/0012-curva-parallela-nel-piano.md) (Decisione, punto 4, e
  il rischio in «Conseguenze»); le intestazioni interne `geom2d/src/exact/`
  (P6), `geom2d/src/curve_meaning.hpp` (P7) e `geom2d/src/carriers.hpp` (P9)

## File

- `geom2d/src/exact/tower.hpp`, `geom2d/src/exact/tower.cpp` (nuovi)
- `geom2d/src/offset_carriers.hpp`, `geom2d/src/offset_carriers.cpp` (nuovi)
- `tests/geom2d/tower_test.cpp`, `tests/geom2d/offset_carriers_test.cpp` (nuovi)

Nessun `CMakeLists.txt`: sorgenti e test li raccoglie la compilazione preparata
da P3.

## Perché serve

La curva parallela di una polilinea (P14) unisce due pezzi spostati nel loro
punto comune, e il centro di un cerchio tangente a due curve (P15) è il punto
comune delle loro curve parallele a distanza r. In entrambi i casi si incontrano
**sostegni spostati**: la retta di un segmento spostata lungo la normale, il
cerchio di un arco con il raggio cambiato. Questi sostegni non hanno più
coefficienti razionali:

- la normale unitaria di un segmento obliquo ha la radice della sua lunghezza
  (per (0, 0)–(1, 2) la retta spostata di d è −2x + y = d·√5);
- il raggio di un pezzo di polilinea curvo è √ρ, con ρ razionale (P7), e quello
  spostato è √ρ ± d.

Una retta spostata contro un cerchio spostato, o due cerchi spostati, danno punti
della forma p ± q·√Δ con p, q e Δ che contengono già quelle radici: **radici
annidate**, che le funzioni a una e due radici di P6 non trattano. E le decisioni
di P14 e P15 confrontano punti di incontri diversi: quale estremo di un pezzo
spostato viene prima (passo 3 dell'offset), l'ordine dei punti di tangenza di
cerchi diversi lungo la prima curva. Nei casi della tappa servono al più cinque
radici insieme: le normali o i raggi di tre pezzi consecutivi e i discriminanti
delle due giunzioni di quello di mezzo.

Per questo il supporto sta in un pacchetto a sé, prima di P14 e P15, che lo
usano senza cambiarlo.

## Cosa fare

Due intestazioni interne, nel namespace `solid_space::geom2d::detail` (e
`::exact` per i numeri). Nomi e firme li fissi tu; le intestazioni li
documentano, perché li usano P14 e P15.

**Numeri di una torre di radici** (`exact/tower.hpp`), sopra i `Dyadic` di P6:

- un numero è un polinomio a coefficienti razionali (diadici, o quozienti di
  diadici in coordinate omogenee) in un insieme di radici √r₁, …, √rₖ, con ogni
  rᵢ a sua volta un numero della torre sotto di lui e non negativo; k è limitato
  (almeno cinque) e il limite sta scritto nell'intestazione;
- somma, differenza, prodotto e **segno esatto**, con il procedimento classico:
  il segno di x + y·√r, con x e y della torre sotto, si ricava dai segni di x, y e
  di x² − y²·r; nessuna soglia, esatto per ogni esponente come P6;
- un arrotondamento a `double` entro 2 ulp, per le costruzioni;
- il costo resta limitato perché la profondità è limitata; il filtro in virgola
  mobile delle decisioni frequenti è di chi le usa.

**Sostegni spostati** (`offset_carriers.hpp`), sopra i sostegni di P9:

- il sostegno di una curva spostato di una distanza con segno (positiva a
  sinistra, come in `offset.hpp`): una retta, oppure un cerchio con lo stesso
  centro e raggio R − s o R + s secondo il verso, come in `offset.hpp`; il
  confronto esatto del nuovo raggio con zero;
- gli **incontri esatti** di due sostegni spostati, o di uno spostato e uno no:
  nessun punto, una tangenza, due punti, lo stesso sostegno, con il `Contact`
  deciso esattamente; i punti come numeri della torre;
- per un punto della torre: il lato rispetto a un sostegno (spostato o no), con
  lo stesso significato di `side()`; l'ordine esatto di due punti lungo un
  sostegno, a partire da un punto dato, con le definizioni di `curves.hpp` (le
  stesse che P10 usa per i suoi punti, in parallelo a questo pacchetto), anche
  quando uno dei due punti ha una o due radici sole; il confronto esatto delle
  distanze di due punti da un punto razionale (il vertice V della giunzione più
  vicina, P14);
- l'arrotondamento di un punto entro `kRelativePointError`.

## Regole della tappa che valgono qui

- **Contratto:** il pacchetto non tocca le intestazioni pubbliche.
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, niente funzioni
  trigonometriche di `<cmath>`.
- Le intestazioni interne di P6, P7 e P9 si usano senza cambiarle; se manca
  qualcosa, fermati e scrivilo nella issue.
- Nessuna eccezione per errori attesi; solo `std::bad_alloc` quando la memoria
  è esaurita (ADR 0003, punto 5, corretto), che non si intercetta. Le funzioni
  interne non sono `noexcept`.
- I file di `tests/corpus/in-attesa/` non ti riguardano: il pacchetto non ha
  casi propri.

## Test richiesti

- `tower_test.cpp`: identità esatte con radici annidate (per esempio
  √(5 + 2√6) − √2 − √3 = 0, (√2 + √3)² − 5 − 2√6 = 0), anche scalate verso i
  subnormali e verso 2^50; numeri a un ulp da zero; segno coerente con
  l'arrotondamento quando il valore è lontano da zero; il limite di profondità
  rispettato.
- `offset_carriers_test.cpp`: per ogni coppia (retta spostata, cerchio spostato,
  sostegno non spostato) incroci, tangenze esatte e a un ulp, nessun incontro,
  stesso sostegno; ogni punto trovato sta **esattamente** su entrambi i sostegni
  (sostituzione nella torre); scambiare i due sostegni dà gli stessi punti;
  spostare di 0 dà gli incontri di P9; il segmento (0, 0)–(1, 2) spostato contro
  un cerchio spostato, e due pezzi curvi spostati, con i valori controllati a
  mano nel test.

## Fatto quando

- Le due intestazioni interne documentano che cosa offrono, compreso il limite
  della torre, e i test sopra passano.
- La CI è verde, anche con AddressSanitizer e UBSan.
