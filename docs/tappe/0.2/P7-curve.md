# [0.2-P7] `geom2d`: curve

- **Tappa:** 0.2 — [piano](../0.2-geometria-del-piano.md)
- **Issue:** #…
- **Difficoltà:** impegnativa
- **Agente:** claude-2
- **Modello e impegno:** forte; impegno alto
- **Riferimenti:** issue `[0.2-R7a]` (#…, dati e curve ridotte a un punto) e
  `[0.2-R7b]` (#…, pezzi, grandezza, estremi, lunghezze), agente codex (calcoli
  delicati: archi al limite degli arrotondamenti)
- **Dipende da:** P5, P6, R7a, R7b
- **Sblocca in Tecnigrafo:** Matleyx78/tecnigrafo#357 (con P4–P6, P8–P11) e #373
  (con P4–P6, P8)
- **Componente:** `geom2d`
- **Da leggere:** [ADR 0010](../../adr/0010-posizione-e-intersezioni-nel-piano.md)
  punti 1, 3–5 e «Che cosa rende possibili decisioni esatte»;
  `geom2d/include/solid_space/geom2d/curves.hpp` (tutto: il significato esatto
  delle curve e il controllo dei dati), `diagnosis.hpp`;
  `robust/include/solid_space/robust/trigonometry.hpp`; le intestazioni interne
  di P6 (`geom2d/src/exact/`)

## File

- `geom2d/src/curve_meaning.hpp`, `geom2d/src/curve_meaning.cpp` (nuovi,
  interni)
- `geom2d/src/curves.cpp` (nuovo)
- `tests/geom2d/curve_meaning_test.cpp`, `tests/geom2d/curves_test.cpp` (nuovi)
- `tests/corpus/esegui/op_curves.cpp` (nuovo)
- `tests/corpus/in-attesa.txt` (togliere i blocchi di R7a e R7b)

## Cosa fare

**Pubblico.** Realizzare, senza cambiarle, le funzioni di `curves.hpp`: `pieces`,
`magnitude`, `endpoints`, `length`, con il controllo dei dati nell'ordine scritto
in "Checking the data" e gli errori dichiarati (`kRelativePointError` per i
punti, `kRelativeStationError` per le lunghezze).

**Interno** (`curve_meaning.hpp`, nel namespace `solid_space::geom2d::detail`):
ciò che serve a tutti i pacchetti successivi per decidere sulle curve.

- `check(const Curve&, std::size_t input)`: la prima diagnosi che si applica,
  oppure nessuna, nell'ordine di `curves.hpp`; una variante che accetta le curve
  degeneri, per `magnitude` e per `coincidence.hpp`.
- Il **significato esatto** di ogni curva, con numeri di P6: il sostegno (retta
  per due punti, o cerchio con centro e raggio al quadrato in coordinate
  omogenee: per un segmento di polilinea curvo, il cerchio per inizio, m e
  fine, con m razionale nei dati), il verso, e per un arco le direzioni D0 e D1
  date da `robust::sinCos` con le quattro regole di `curves.hpp` (cerchio intero,
  un punto, alpha), decise esattamente da D0 e D1 senza trigonometria.
- Le domande di base che P8–P16 rifanno spesso: se una direzione dal centro sta
  sull'arco, prima o dopo; la grandezza M di più curve.

Il significato esatto si calcola una volta per curva e si riusa: le funzioni
interne prendono la forma già calcolata.

### Casi di verifica (per R7a e R7b)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `pieces` | `polilinea` | `pezzi`: elenco di `{"inizio": punto, "fine": punto, "bulge": esatto}`, valori esatti (sono copie) |
| `magnitude` | `curva` | `grandezza`: esatto; per un pezzo curvo approssimato con tolleranza 1 ulp (m arrotondato a `double`); oppure `diagnosi` |
| `endpoints` | `curva` | `inizio`, `fine`: punti approssimati, tolleranza `kRelativePointError · max(M, |p|)`; oppure `diagnosi` |
| `length` | `curva` | `lunghezza`: approssimato, tolleranza `kRelativeStationError · max(M, |s|)`; oppure `diagnosi` |

**R7a** copre i dati e le curve ridotte a un punto, **R7b** il resto; almeno:

- **dati** (R7a): per ogni tipo, NaN, infinito, raggio negativo, coordinata o raggio
  oltre 2^50, angolo oltre 2^20, punto m di un pezzo curvo oltre il dominio; due
  difetti insieme, per l'ordine delle diagnosi; `dato` è sempre `"0"`;
- **curve ridotte a un punto** (R7a): segmento con inizio uguale alla fine, raggio
  zero, pezzo con inizio uguale alla fine, arco di ampiezza 0, arco in cui
  `angolo_iniziale + ampiezza` arrotondato è uguale ad `angolo_iniziale` (per
  esempio angolo 1e6 e ampiezza 1e-12: D0 e D1 sono lo stesso `double`);
- **archi interi** (R7b): |ampiezza| uguale o oltre `kFullTurn`, positiva e negativa;
- `pieces` (R7b) di polilinee aperte e chiuse, con zero, uno e due vertici, con bulge
  diversi;
- `magnitude` di ogni tipo, anche di un pezzo curvo il cui punto m sta fuori dal
  rettangolo degli estremi;
- estremi e lunghezze di archi di un quarto, mezzo e tre quarti di giro nei due
  versi, di pezzi con bulge 1, −1 e piccolo, di cerchi, di segmenti, anche a
  coordinate di 1e6.

Gli archi seguono la regola del README («Gli archi»): nessun caso dipende da
dove `sinCos` arrotonda entro 2 ulp. Seni, coseni e arcotangenti esatti vengono
da serie calcolate con `decimal` di Python; i razionali da `fractions`.

## Regole della tappa che valgono qui

- **Contratto:** le intestazioni non cambiano, commenti compresi. Se qualcosa
  manca o non torna, fermati e scrivilo nella issue (etichetta `contratto`).
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`; niente funzioni
  trigonometriche di `<cmath>`: seni, coseni e angoli vengono da `robust`.
- Decisioni esatte, costruzioni con l'errore dichiarato, nessuna soglia.
- `curve_meaning.hpp` è un'intestazione interna che P8–P16 useranno: tienila
  documentata.
- File comuni: solo `tests/corpus/in-attesa.txt`, dove togli i blocchi dei tuoi
  riferimenti e nient'altro (piano, sezione 6). Nessun `CMakeLists.txt`: sorgenti
  e test li raccoglie la compilazione preparata da P3 (i test di unità in
  `tests/<componente>/`, le operazioni dei casi in `tests/corpus/esegui/`).

## Test richiesti

- `curve_meaning_test.cpp`: le quattro regole degli archi, con D0 e D1 costruiti
  a mano (senza passare da `sinCos`) anche ai bordi; il cerchio di un pezzo curvo
  passa esattamente per inizio, m e fine; la direzione sull'arco nei due versi.
- `curves_test.cpp`: `endpoints` coerente con il significato esatto (ogni
  estremo calcolato è entro l'errore dichiarato dal punto del cerchio in
  direzione D0 o D1); `length` di un cerchio intero entro l'errore da 2πr;
  `pieces` di una polilinea chiusa ha come ultimo pezzo il ritorno al primo
  vertice, con il bulge dell'ultimo.
- I casi di R7a e R7b passano.

## Fatto quando

- `pieces`, `magnitude`, `endpoints` e `length` sono realizzate come dichiara
  `curves.hpp`, con il controllo dei dati nell'ordine scritto lì.
- Il significato esatto delle curve è nell'intestazione interna, documentato.
- I casi di `[0.2-R7a]` e `[0.2-R7b]` passano e i loro
  blocchi sono tolti da `in-attesa.txt`.
- Il job del determinismo è verde.
- La CI è verde.
