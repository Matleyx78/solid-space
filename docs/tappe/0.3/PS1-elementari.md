# [0.3-PS1] `robust`: tangente, arcocoseno e lunghezza di un vettore

- **Tappa:** 0.3 — [piano](../0.3-regioni-ed-ellisse.md)
- **Issue:** #…
- **Difficoltà:** media
- **Agente:** codex
- **Modello e impegno:** medio; impegno alto (tabella in
  LAVORO-IN-PARALLELO.md). La revisione di Claude è mirata, in fascia forte,
  sull'analisi dell'errore vicino ai poli e vicino a ±1
- **Riferimenti:** issue `[0.3-RS1a]` (#…), agente claude, fascia forte
  (`tan` e `acos`); `[0.3-RS1b]` (#…), agente claude, fascia leggera (`hypot`)
- **Contratto:** `robust/include/solid_space/robust/elementary.hpp` (`tan`,
  `acos`, `hypot`, `kElementaryMaxErrorUlps`, `kMaxHypotArgument`)
- **Dipende da:** la revisione del contratto `[0.3-CS]` (#222); RS1a (#…),
  RS1b (#…)
- **Sblocca in Tecnigrafo:** PT41 (issue ancora da aprire): `core` e `cmd`
  passano a `robust::tan`, `robust::acos` e `robust::hypot`, e l'elenco delle
  eccezioni del confronto tra sistemi si svuota
- **Componente:** `robust`
- **Da leggere:** [ADR 0016](../../adr/0016-tangente-arcocoseno-e-lunghezza-deterministici.md),
  con la riga **Corretto:** (#230); `elementary.hpp` e `trigonometry.hpp`;
  `robust/src/trigonometry.cpp` (riduzione, double-double, arcotangente, e la
  sua analisi dell'errore, da cui riparti) e `trigonometry_constants.hpp`;
  [ADR 0008](../../adr/0008-componente-geom2d.md), punto 5

## File

- `robust/src/elementary.cpp` (nuovo)
- `robust/src/trigonometry_internal.hpp` (nuovo): le funzioni di supporto di
  `trigonometry.cpp` che servono anche qui (double-double, riduzione,
  arcotangente), spostate senza cambiarle
- `robust/src/trigonometry.cpp`: solo lo spostamento delle funzioni di
  supporto; `sinCos` e `atan2` non cambiano, né nel codice né nei bit
- `tests/robust/elementary_test.cpp` (nuovo)
- `tests/corpus/esegui/op_elementary.cpp` (nuovo)
- `tests/corpus/in-attesa/`: cancellare `0.3-RS1a.txt` e `0.3-RS1b.txt`

Nessun `CMakeLists.txt`: la raccolta dei sorgenti li prende da sé.

## Cosa fare

Realizzare, senza cambiarla, `elementary.hpp`:

```cpp
inline constexpr double kElementaryMaxErrorUlps = 2.0;
inline constexpr double kMaxHypotArgument = 0x1p1023;
[[nodiscard]] double tan(double angle) noexcept;
[[nodiscard]] double acos(double x) noexcept;
[[nodiscard]] double hypot(double x, double y) noexcept;
```

- **`tan`**: come suggerisce l'ADR 0016 («Su che cosa si basa»), il quoziente
  in double-double di seno e coseno dello stesso angolo ridotto, con la
  riduzione di `sinCos`; dispari bit per bit, ±0 per ±0, sempre finita.
- **`acos`**: per esempio come arcotangente di √((1 − x)(1 + x)) / x con
  l'arcotangente di `trigonometry.cpp`; esatti i due estremi (`acos(1)` è +0,
  `acos(−1)` il `double` più vicino a π), risultato mai fuori da
  [+0, π arrotondato], errore relativo anche vicino a 1.
- **`hypot`**: la somma dei quadrati in double-double (prodotti esatti con
  `fma`) dopo una scala per una potenza di 2, e una correzione della radice;
  esatta quando la lunghezza è un `double`, mai sotto la componente maggiore,
  uguale per segni e ordine degli argomenti, +0 solo per due zeri.
- Precondizioni controllate in debug, come `sinCos` e `atan2`.
- Il commento in testa a `elementary.cpp` porta l'analisi dell'errore di ogni
  funzione, come quello di `trigonometry.cpp`, e dice perché ogni promessa per
  struttura vale.

Il commento di stato di `elementary.hpp` («Declared, not yet implemented») non
si tocca: lo aggiorna PS11.

### Casi di verifica (per RS1a e RS1b)

| Operazione | `ingresso` | `atteso` |
|---|---|---|
| `tan` | `angolo` | `valore` (esatto o approssimato), `valore_negativo` (booleano) |
| `acos` | `x` | `angolo`, `angolo_negativo` |
| `hypot` | `x`, `y` | `lunghezza`, `lunghezza_negativa` |

Le chiavi `_negativo` portano il bit del segno, che il confronto esatto dei
numeri ignora (come in `op_trigonometry.cpp`). I valori approssimati hanno
tolleranza 2 ulp del valore esatto (2^-1074 per un risultato subnormale); i
valori che il contratto fissa per struttura si scrivono esatti. L'operazione
`hypot` controlla anche che gli argomenti scambiati e con i segni cambiati
diano gli stessi bit.

**RS1a** (fascia forte) copre almeno:

- `tan`: multipli di π/8 e π/4 (il `double` di π/8 dei bulge da 90°); angoli
  vicini a π/2 (un quarto di un'ampiezza vicina a 2π); il `double` più vicino a
  un polo fino a 2^20, dalla ricerca esaustiva citata in `trigonometry.cpp`;
  ±0; angoli sotto 2^-27; gli angoli di obliquità del testo (15°, 30°);
  `kMaxTrigArgument` e il suo opposto;
- `acos`: ±1, ±0, ±1/2, 1 − 2^-53 e −1 + 2^-53; valori 1 − tol / r del passo
  degli archi (per esempio tol 0,01 e r 1, 100, 1e5); numeri sotto 2^-27;
- valori con il modulo `decimal` a molte cifre (almeno 60), come per `sinCos` e
  `atan2`, con il procedimento in `provenienza`.

**RS1b** (fascia leggera) copre almeno:

- `hypot`: le terne (3, 4, 5) e (5, 12, 13), anche scalate per potenze di 2 fino
  ai subnormali e fino a vicino `kMaxHypotArgument`; (x, ±0) e (±0, ±0), esatti;
  (1e-12, 1e6); componenti uguali (a·√2); componenti subnormali;
  `kMaxHypotArgument` con 0 e con sé stesso;
- il vettore nullo: `hypot(±0, ±0)` è +0, esatto (#230).

## Regole della tappa che valgono qui

- **Contratto:** `elementary.hpp` e `trigonometry.hpp` non cambiano, commenti
  compresi. Se qualcosa manca o non torna, fermati come dice AGENTS.md
  («Un buco nel contratto»).
- **Determinismo** (ADR 0008, punto 5): calcolo nei `.cpp`, solo operazioni che
  IEEE 754 arrotonda correttamente (`+`, `−`, `×`, `/`, `sqrt`, `fma`, `round`,
  `fabs`, `copysign`); niente `<cmath>` per tangente, arcocoseno o `hypot`.
- I bit di `sinCos` e `atan2` non cambiano: i loro casi della 0.2 lo provano.
- File comuni: nessuno; cancelli solo i file dei tuoi riferimenti in
  `tests/corpus/in-attesa/`.

## Test richiesti

- `elementary_test.cpp`: le promesse per struttura su dati generati in modo
  deterministico (`tan` dispari e finita; `acos` dentro [+0, π arrotondato] e
  agli estremi; `hypot` simmetrica, mai sotto la componente maggiore, esatta
  sulle terne pitagoriche, +0 solo per due zeri; `a / hypot(a, b)` dentro
  [−1, 1] per vettori non nulli); l'accordo con `<cmath>` entro qualche ulp,
  solo nei test.
- I casi di RS1a e RS1b passano.

## Fatto quando

- `tan`, `acos` e `hypot` sono realizzate come dichiara `elementary.hpp`, con
  l'analisi dell'errore nel commento.
- I casi di `[0.3-RS1a]` e `[0.3-RS1b]` passano e i loro file sono cancellati da
  `in-attesa/`.
- I casi della 0.2 di `sinCos` e `atan2` passano senza cambiamenti.
- Il job del determinismo è verde.
- La CI è verde.
