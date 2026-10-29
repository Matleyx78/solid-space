# [0.2-P6] `geom2d`: numeri esatti

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #…
- **Difficoltà:** impegnativa
- **Agente:** claude
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** non servono: il pacchetto non ha funzioni pubbliche; si prova
  con i suoi test di unità e, attraverso le funzioni pubbliche, con i casi di
  R7–R15
- **Dipende da:** P3
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#357 (con P4, P5, P7–P11) e
  #373 (con P4, P5, P7, P8)
- **Componente:** `geom2d` (interno)
- **Da leggere:** [ADR 0010](../../adr/0010-posizione-e-intersezioni-nel-piano.md)
  («Che cosa rende possibili decisioni esatte»),
  [ADR 0011](../../adr/0011-raccordi-e-tangenze-nel-piano.md) (lo stesso
  paragrafo), [ADR 0008](../../adr/0008-componente-geom2d.md) punti 5 e 6;
  `robust/include/solid_space/robust/predicates.hpp` (il tipo `Sign`)

## File

- `geom2d/src/exact/dyadic.hpp`, `geom2d/src/exact/dyadic.cpp` (nuovi)
- `geom2d/src/exact/quadratic.hpp`, `geom2d/src/exact/quadratic.cpp` (nuovi)
- `geom2d/src/geom2d.cpp` (si toglie: era il segnaposto)
- `geom2d/CMakeLists.txt`
- `tests/geom2d/exact_test.cpp` (nuovo)
- `tests/CMakeLists.txt` (un eseguibile per i test interni di `geom2d`, che
  vede `geom2d/src/`)

## Cosa fare

Il calcolo esatto su cui poggiano tutte le decisioni di `geom2d`. Ogni coordinata
che le decisioni confrontano è della forma (a + b·√d) / c, o una combinazione di
due radici, con a, b, c, d polinomi nei `double` ricevuti (ADR 0010 e 0011,
«Che cosa rende possibili decisioni esatte»). In coordinate omogenee non serve
la divisione: bastano somme, prodotti e segni.

**Requisito che decide la rappresentazione:** il calcolo è esatto **per ogni
esponente**. I dati vanno da 2^-1074 a 2^50 e i prodotti hanno grado fino a una
decina: una rappresentazione legata all'intervallo degli esponenti dei `double`
(come le espansioni) perde i prodotti minuscoli. Da qui i numeri diadici: un
intero di grandezza qualsiasi per una potenza di due.

L'interfaccia interna, nel namespace `solid_space::geom2d::exact`; i nomi e le
firme li fissi tu, ma devono offrire almeno questo e restare documentati nelle
due intestazioni, perché li usano P7–P15:

```cpp
namespace solid_space::geom2d::exact {

/// An exact dyadic number: an integer of any size times a power of two. Every
/// finite double is one; sums, differences and products are exact for any
/// exponents, with no underflow and no overflow.
class Dyadic;
// Dyadic(double) for a finite double; +, -, unary -, *; sign(); compare();
// toDouble(): the nearest double (ties to even).

/// The nearest double to numerator / denominator. Precondition: denominator
/// is not zero.
double quotientToDouble(const Dyadic& numerator, const Dyadic& denominator);

/// The sign of a + b * sqrt(d). Precondition: d >= 0.
robust::Sign signOf(const Dyadic& a, const Dyadic& b, const Dyadic& d);

/// The sign of x + y * sqrt(e), where x and y are numbers of the form
/// p + q * sqrt(d): two different radicals, and their product, as when points
/// of different pairs are compared along a curve (ADR 0010, intersectAlong).
/// Preconditions: d >= 0, e >= 0.
robust::Sign signOf(const QuadraticNumber& x, const QuadraticNumber& y, const Dyadic& e);

/// A double within 2 ulp of (a + b * sqrt(d)) / c, for the constructions.
/// Precondition: d >= 0, c != 0.
double quadraticToDouble(const Dyadic& a, const Dyadic& b, const Dyadic& d, const Dyadic& c);
}
```

- **Prestazioni:** i numeri piccoli (uno o due "limb") non allocano memoria; il
  filtro in virgola mobile delle singole decisioni lo scrivono i pacchetti che le
  fanno, non questo.
- **Radici annidate** (rette spostate contro cerchi spostati, ADR 0011) non
  servono qui: se servono, le aggiunge P14 in un suo file.
- Togli il segnaposto `geom2d/src/geom2d.cpp`: da qui `geom2d` ha codice vero.

## Regole della tappa che valgono qui

- **Contratto:** il pacchetto non tocca le intestazioni pubbliche di `geom2d`.
- **Determinismo** (ADR 0008, punto 5): tutto il calcolo nei `.cpp`; le
  intestazioni interne dichiarano, non calcolano in virgola mobile; le opzioni
  dei target le ha messe P3.
- Nessuna eccezione esce da `geom2d`: un'allocazione fallita è l'unico caso, e
  resta quello della libreria standard.
- File comuni: `geom2d/CMakeLists.txt`, `tests/CMakeLists.txt` (solo le tue
  righe). L'eseguibile dei test interni lo riusano P7–P15.

## Test richiesti

- `Dyadic`: ogni `double` finito, subnormali e ±0 compresi, torna se stesso con
  `toDouble`; identità ((x + y)·(x − y) = x² − y², associatività) su dati
  generati in modo deterministico con esponenti da −1074 a 1023; prodotti che in
  `double` andrebbero sotto i subnormali hanno il segno giusto.
- `quotientToDouble` e `toDouble` arrotondano al più vicino, con i casi a metà.
- `signOf` con una radice: quadrati perfetti, d = 0, b = 0, a e b√d quasi uguali
  e opposti; con due radici: casi costruiti a mano in cui la somma è esattamente
  zero (per esempio √2 + √8 − 3√2) e casi a un ulp da zero.
- `quadraticToDouble` entro 2 ulp, confrontato con un calcolo a precisione più
  alta fatto nel test con `Dyadic`.

## Fatto quando

- Le intestazioni interne `dyadic.hpp` e `quadratic.hpp` documentano che cosa
  offrono, e i test sopra passano.
- Il segnaposto di `geom2d` è tolto.
- La CI è verde, anche con AddressSanitizer e UBSan.
